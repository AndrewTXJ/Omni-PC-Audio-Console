// The lock-free parameter queue (roadmap 4.6: "parameters arrive over lock-free
// queues").
//
// Concurrency code that has only been exercised single-threaded is untested
// concurrency code, so this runs a real producer thread against a real consumer
// thread and checks the three things that can go wrong: a lost command, a
// duplicated command, and a reordered one. A dropped mute is the failure that
// matters -- it leaves a stream live when the user pressed the button.
#include <atomic>
#include <thread>
#include <vector>

#include "omni/control/command.hpp"
#include "omni/engine/graph.hpp"
#include "test_support.hpp"

using namespace omni;
using namespace omni::test;

int main() {
    begin("lock-free command queue");

    // --- single-threaded basics ------------------------------------------
    {
        CommandQueue<8> q;
        OMNI_CHECK(q.empty());
        OMNI_CHECK(q.size() == 0);
        OMNI_CHECK(q.capacity() == 7);  // one slot is the full/empty discriminator

        Command c;
        OMNI_CHECK(!q.pop(c));  // empty

        for (std::uint32_t i = 0; i < 7; ++i)
            OMNI_CHECK(q.push({CommandType::StripFaderDb, i, 0, static_cast<float>(i), false}));
        OMNI_CHECK(q.size() == 7);
        // Full: a push must FAIL rather than overwrite an unread command.
        OMNI_CHECK(!q.push({CommandType::StripMute, 99, 0, 0.0f, true}));

        for (std::uint32_t i = 0; i < 7; ++i) {
            OMNI_CHECK(q.pop(c));
            OMNI_CHECK(c.target == i);          // FIFO order
            OMNI_CHECK_EXACT(c.value, static_cast<float>(i));
        }
        OMNI_CHECK(q.empty());
        OMNI_CHECK(!q.pop(c));
        // Usable again after draining: the indices wrap.
        OMNI_CHECK(q.push({CommandType::StripMute, 1, 0, 0.0f, true}));
        OMNI_CHECK(q.pop(c));
        OMNI_CHECK(c.type == CommandType::StripMute);
    }

    // --- two real threads -------------------------------------------------
    {
        DefaultCommandQueue q;
        constexpr std::uint32_t kTotal = 200000;
        std::atomic<bool> producer_done{false};
        std::vector<std::uint32_t> received;
        received.reserve(kTotal);
        std::uint64_t pushes_refused = 0;

        std::thread consumer([&] {
            Command c;
            while (received.size() < kTotal) {
                if (q.pop(c)) {
                    received.push_back(c.target);
                } else if (producer_done.load(std::memory_order_acquire) && q.empty()) {
                    break;
                }
            }
        });

        for (std::uint32_t i = 0; i < kTotal; ++i) {
            // Retry on a full queue rather than dropping: this mirrors what a UI
            // should do, and makes loss attributable to the queue if it occurs.
            while (!q.push({CommandType::StripFaderDb, i, 0, static_cast<float>(i), false}))
                ++pushes_refused;
        }
        producer_done.store(true, std::memory_order_release);
        consumer.join();

        char msg[160];
        std::snprintf(msg, sizeof msg, "received %zu of %u commands",
                      received.size(), kTotal);
        OMNI_CHECK_MSG(received.size() == kTotal, msg);

        // Strict FIFO with no loss, duplication or reordering.
        std::size_t out_of_order = 0;
        for (std::size_t i = 0; i < received.size(); ++i)
            if (received[i] != static_cast<std::uint32_t>(i)) ++out_of_order;
        std::snprintf(msg, sizeof msg, "%zu command(s) lost, duplicated or reordered",
                      out_of_order);
        OMNI_CHECK_MSG(out_of_order == 0, msg);

        // The queue really did fill up, so the full path above was exercised
        // rather than merely present.
        std::snprintf(msg, sizeof msg,
                      "queue never filled (%llu refusals); the full-queue path was "
                      "not exercised", static_cast<unsigned long long>(pushes_refused));
        OMNI_CHECK_MSG(pushes_refused > 0, msg);
    }

    // --- commands actually change the graph -------------------------------
    {
        Graph g;
        g.configure({48000.0, 64, 2, 2});
        g.snap();

        apply(g, {CommandType::StripFaderDb, 0, 0, -6.0f, false});
        OMNI_CHECK_EXACT(g.strip(0).fader_db(), -6.0f);
        apply(g, {CommandType::StripTrimDb, 1, 0, 12.0f, false});
        OMNI_CHECK_EXACT(g.strip(1).trim_db(), 12.0f);
        apply(g, {CommandType::StripMute, 0, 0, 0.0f, true});
        OMNI_CHECK(g.strip(0).muted());
        apply(g, {CommandType::StripPan, 0, 0, -1.0f, false});
        OMNI_CHECK_EXACT(g.strip(0).pan(), -1.0f);
        apply(g, {CommandType::StripPolarity, 0, 0, 0.0f, true});
        OMNI_CHECK(g.strip(0).polarity_invert());
        apply(g, {CommandType::StripSendEnabled, 1, 1, 0.0f, true});
        OMNI_CHECK(g.strip(1).send_enabled(1));
        apply(g, {CommandType::BusFaderDb, 1, 0, -3.0f, false});
        OMNI_CHECK_EXACT(g.bus(1).fader_db(), -3.0f);
        apply(g, {CommandType::BusMono, 0, 0, 0.0f, true});
        OMNI_CHECK(g.bus(0).mono());

        // Panic mutes every bus, which is the whole point of it (4.11).
        apply(g, {CommandType::BusMute, 0, 0, 0.0f, false});
        apply(g, {CommandType::BusMute, 1, 0, 0.0f, false});
        apply(g, {CommandType::PanicMute, 0, 0, 0.0f, true});
        for (std::size_t b = 0; b < g.num_buses(); ++b) OMNI_CHECK(g.bus(b).muted());

        // Out-of-range targets are ignored, not undefined behaviour: a stale
        // command from a client that still thinks there are more strips must not
        // corrupt the graph.
        const float fader_before = g.strip(0).fader_db();   // -6 dB, set above
        apply(g, {CommandType::StripFaderDb, 9999, 0, -20.0f, false});
        apply(g, {CommandType::BusMute, 9999, 0, 0.0f, true});
        apply(g, {CommandType::StripSendEnabled, 0, 9999, 0.0f, true});
        OMNI_CHECK_EXACT(g.strip(0).fader_db(), fader_before);
        OMNI_CHECK(g.num_strips() == 2);
    }

    return finish();
}
