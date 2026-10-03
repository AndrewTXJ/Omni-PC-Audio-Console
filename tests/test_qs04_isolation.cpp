// QS-04 Channel isolation -- the M1-reachable subset (docs/TEST-PLAN.md).
//
// Target: bit-exact silence on unpatched channels.
//
// SCOPE. The full test requires the many-to-one and one-to-many patches of the
// Patch window (4.2), which arrives at M3. What exists at M1 is the send matrix,
// so this covers routing through the A/B buttons: a bus nobody sends to, and a
// bus fed only by silence, must both be exactly zero. The test plan already says
// QS-04 is not complete until M3; this is the part that can be held now.
#include <vector>

#include "omni/engine/graph.hpp"
#include "signals.hpp"
#include "test_support.hpp"

using namespace omni;
using namespace omni::test;

namespace {

constexpr double kRate = 48000.0;
constexpr std::size_t kBlock = 64;
constexpr std::size_t kStrips = 4;
constexpr std::size_t kBuses = 4;

void check_exact_silence(const Graph& g, std::size_t bus, std::size_t frames,
                         const char* what) {
    std::size_t nonzero = 0;
    float worst = 0.0f;
    for (std::size_t c = 0; c < kChannels; ++c) {
        const float* p = g.bus_output(bus, c);
        for (std::size_t i = 0; i < frames; ++i) {
            if (p[i] != 0.0f) {
                ++nonzero;
                worst = std::max(worst, std::fabs(p[i]));
            }
        }
    }
    char msg[200];
    std::snprintf(msg, sizeof msg, "%s (bus %zu): %zu non-zero samples, worst %.9g",
                  what, bus, nonzero, static_cast<double>(worst));
    OMNI_CHECK_MSG(nonzero == 0, msg);
}

}  // namespace

int main() {
    begin("QS-04 channel isolation (M1 subset: send matrix)");

    const std::vector<float> loud = sine(kBlock, 1000.0, kRate, -1.0f);

    // Drive each strip in turn, routed to its own bus only. Every other bus must
    // be exactly zero -- including buses fed by silent-but-routed strips, which
    // is where an accumulation bug would show up.
    for (std::size_t driven = 0; driven < kStrips; ++driven) {
        Graph g;
        g.configure({kRate, kBlock, kStrips, kBuses});
        for (std::size_t s = 0; s < kStrips; ++s) g.strip(s).set_send_enabled(s, true);

        for (int block = 0; block < 40; ++block) {
            g.clear_inputs();
            std::copy(loud.begin(), loud.end(), g.strip_input(driven, 0));
            std::copy(loud.begin(), loud.end(), g.strip_input(driven, 1));
            g.process(kBlock);
        }
        for (std::size_t b = 0; b < kBuses; ++b) {
            if (b == driven) {
                OMNI_CHECK(peak(g.bus_output(b, 0), kBlock) > 0.5f);
            } else {
                check_exact_silence(g, b, kBlock, "silent-source bus");
            }
        }
    }

    // A bus nobody sends to at all, while other routing is active.
    {
        Graph g;
        g.configure({kRate, kBlock, kStrips, kBuses});
        g.strip(0).set_send_enabled(0, true);
        g.strip(1).set_send_enabled(0, true);   // many-to-one
        g.strip(2).set_send_enabled(1, true);
        g.strip(2).set_send_enabled(2, true);   // one-to-many
        for (int block = 0; block < 40; ++block) {
            g.clear_inputs();
            for (std::size_t s = 0; s < kStrips; ++s) {
                std::copy(loud.begin(), loud.end(), g.strip_input(s, 0));
                std::copy(loud.begin(), loud.end(), g.strip_input(s, 1));
            }
            g.process(kBlock);
        }
        OMNI_CHECK(peak(g.bus_output(0, 0), kBlock) > 0.5f);
        OMNI_CHECK(peak(g.bus_output(1, 0), kBlock) > 0.5f);
        OMNI_CHECK(peak(g.bus_output(2, 0), kBlock) > 0.5f);
        check_exact_silence(g, 3, kBlock, "unrouted bus");
    }

    // A muted strip contributes exactly nothing once its ramp has settled.
    {
        Graph g;
        g.configure({kRate, kBlock, 1, 1});
        g.strip(0).set_send_enabled(0, true);
        g.strip(0).set_mute(true);
        for (int block = 0; block < 40; ++block) {
            std::copy(loud.begin(), loud.end(), g.strip_input(0, 0));
            std::copy(loud.begin(), loud.end(), g.strip_input(0, 1));
            g.process(kBlock);
        }
        check_exact_silence(g, 0, kBlock, "muted strip");
    }

    // A fader at -inf likewise.
    {
        Graph g;
        g.configure({kRate, kBlock, 1, 1});
        g.strip(0).set_send_enabled(0, true);
        g.strip(0).set_fader_db(-200.0f);
        for (int block = 0; block < 40; ++block) {
            std::copy(loud.begin(), loud.end(), g.strip_input(0, 0));
            std::copy(loud.begin(), loud.end(), g.strip_input(0, 1));
            g.process(kBlock);
        }
        check_exact_silence(g, 0, kBlock, "fader at -inf");
    }

    return finish();
}
