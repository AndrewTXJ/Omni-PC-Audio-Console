// The Patch window (roadmap 4.2) and loop protection.
//
// This is what completes QS-04: the test plan recorded that the many-to-one and
// one-to-many cases needed the Patch window, which did not exist at M1. It does
// now, so they are tested here.
//
// It also covers what 4.2 and section 10 call for directly: "A patch that would
// create a feedback loop is blocked, with an explanation."
#include <string>
#include <vector>

#include "omni/engine/graph.hpp"
#include "signals.hpp"
#include "test_support.hpp"

using namespace omni;
using namespace omni::test;

namespace {

constexpr double kRate = 48000.0;
constexpr std::size_t kBlock = 64;

Endpoint bus_out(std::uint32_t b, std::uint32_t c) {
    return {EndpointKind::BusOutput, b, c};
}
Endpoint strip_in(std::uint32_t s, std::uint32_t c) {
    return {EndpointKind::StripInput, s, c};
}

void run_blocks(Graph& g, const std::vector<float>& src, std::size_t strip, int blocks) {
    for (int i = 0; i < blocks; ++i) {
        g.clear_inputs();
        if (strip < g.num_strips()) {
            std::copy(src.begin(), src.end(), g.strip_input(strip, 0));
            std::copy(src.begin(), src.end(), g.strip_input(strip, 1));
        }
        g.process(kBlock);
    }
}

void check_exact_silence(const Graph& g, std::size_t bus, std::size_t frames,
                         const char* what) {
    std::size_t nonzero = 0;
    float worst = 0.0f;
    for (std::size_t c = 0; c < kChannels; ++c) {
        const float* p = g.bus_output(bus, c);
        for (std::size_t i = 0; i < frames; ++i)
            if (p[i] != 0.0f) {
                ++nonzero;
                worst = std::max(worst, std::fabs(p[i]));
            }
    }
    char msg[200];
    std::snprintf(msg, sizeof msg, "%s (bus %zu): %zu non-zero, worst %.9g", what,
                  bus, nonzero, static_cast<double>(worst));
    OMNI_CHECK_MSG(nonzero == 0, msg);
}

}  // namespace

int main() {
    begin("Patch window and loop protection (completes QS-04)");

    const std::vector<float> tone = sine(kBlock, 1000.0, kRate, -12.0f);
    std::string why;

    // --- validation -------------------------------------------------------
    {
        Graph g;
        g.configure({kRate, kBlock, 2, 2});
        // A patch runs bus -> strip. The reverse is refused rather than silently
        // reinterpreted.
        OMNI_CHECK(g.add_patch(strip_in(0, 0), bus_out(0, 0), 0.0f, why) ==
                   PatchResult::WrongDirection);
        OMNI_CHECK(g.add_patch(bus_out(9, 0), strip_in(0, 0), 0.0f, why) ==
                   PatchResult::OutOfRange);
        OMNI_CHECK(g.add_patch(bus_out(0, 0), strip_in(9, 0), 0.0f, why) ==
                   PatchResult::OutOfRange);
        OMNI_CHECK(g.add_patch(bus_out(0, 9), strip_in(0, 0), 0.0f, why) ==
                   PatchResult::OutOfRange);
        OMNI_CHECK(g.add_patch(bus_out(0, 0), strip_in(1, 0), 0.0f, why) ==
                   PatchResult::Ok);
        OMNI_CHECK(g.add_patch(bus_out(0, 0), strip_in(1, 0), 0.0f, why) ==
                   PatchResult::Duplicate);
    }

    // --- loop protection: the direct case --------------------------------
    // Strip 0 sends to bus 0; patching bus 0 back into strip 0 closes the loop.
    {
        Graph g;
        g.configure({kRate, kBlock, 2, 2});
        g.strip(0).set_send_enabled(0, true);
        g.snap();
        const PatchResult r = g.add_patch(bus_out(0, 0), strip_in(0, 0), 0.0f, why);
        OMNI_CHECK(r == PatchResult::WouldLoop);
        // 4.2 requires an explanation, not just a refusal.
        OMNI_CHECK_MSG(!why.empty(), "a refused patch must explain itself");
        OMNI_CHECK_MSG(why.find("loop") != std::string::npos,
                       "the explanation should name the loop");
        // And the graph is unchanged: no half-added patch left behind.
        OMNI_CHECK(g.patch_bay().entries().empty());
    }

    // --- loop protection: the indirect case ------------------------------
    // strip0 -> bus0 -> strip1 -> bus1 -> strip0 is a cycle of length four, which
    // a check that only looked one hop would miss.
    {
        Graph g;
        g.configure({kRate, kBlock, 2, 2});
        g.strip(0).set_send_enabled(0, true);
        g.strip(1).set_send_enabled(1, true);
        g.snap();
        OMNI_CHECK(g.add_patch(bus_out(0, 0), strip_in(1, 0), 0.0f, why) ==
                   PatchResult::Ok);
        // This one closes it.
        OMNI_CHECK(g.add_patch(bus_out(1, 0), strip_in(0, 0), 0.0f, why) ==
                   PatchResult::WouldLoop);
        OMNI_CHECK(g.patch_bay().entries().size() == 1);
    }

    // --- an A/B button can close a loop too ------------------------------
    // bus0 -> strip1 exists; enabling strip1 -> bus0 would close it, so the UI
    // must be able to refuse the button for the same reason it refuses a patch.
    {
        Graph g;
        g.configure({kRate, kBlock, 2, 2});
        g.snap();
        OMNI_CHECK(g.add_patch(bus_out(0, 0), strip_in(1, 0), 0.0f, why) ==
                   PatchResult::Ok);
        OMNI_CHECK_MSG(g.send_would_loop(1, 0), "strip1 -> bus0 would close a loop");
        OMNI_CHECK_MSG(!g.send_would_loop(1, 1), "strip1 -> bus1 is harmless");
        OMNI_CHECK_MSG(!g.send_would_loop(0, 0), "strip0 -> bus0 is harmless");
    }

    // --- a legal patch carries audio, in dependency order ----------------
    // strip0 -> bus0, bus0 -> strip1, strip1 -> bus1. Bus 1 must receive the
    // signal in the SAME block, which only works if the schedule orders
    // strip0, bus0, strip1, bus1.
    {
        Graph g;
        g.configure({kRate, kBlock, 2, 2});
        g.strip(0).set_send_enabled(0, true);
        g.strip(1).set_send_enabled(1, true);
        OMNI_CHECK(g.add_patch(bus_out(0, 0), strip_in(1, 0), 0.0f, why) ==
                   PatchResult::Ok);
        OMNI_CHECK(g.add_patch(bus_out(0, 1), strip_in(1, 1), 0.0f, why) ==
                   PatchResult::Ok);
        g.snap();

        // The schedule really is in dependency order.
        const auto& sched = g.schedule();
        OMNI_CHECK(sched.size() == 4);
        auto position = [&](ScheduleStep::Kind k, std::uint32_t i) {
            for (std::size_t p = 0; p < sched.size(); ++p)
                if (sched[p].kind == k && sched[p].index == i) return p;
            return sched.size();
        };
        const std::size_t p_s0 = position(ScheduleStep::Kind::Strip, 0);
        const std::size_t p_b0 = position(ScheduleStep::Kind::Bus, 0);
        const std::size_t p_s1 = position(ScheduleStep::Kind::Strip, 1);
        const std::size_t p_b1 = position(ScheduleStep::Kind::Bus, 1);
        OMNI_CHECK_MSG(p_s0 < p_b0, "strip 0 must run before the bus it feeds");
        OMNI_CHECK_MSG(p_b0 < p_s1, "bus 0 must run before the strip it patches to");
        OMNI_CHECK_MSG(p_s1 < p_b1, "strip 1 must run before the bus it feeds");

        run_blocks(g, tone, 0, 20);
        // Both buses carry the tone, in one block, with no extra delay.
        OMNI_CHECK(peak(g.bus_output(0, 0), kBlock) > 0.1f);
        OMNI_CHECK_MSG(peak(g.bus_output(1, 0), kBlock) > 0.1f,
                       "the patched path must reach bus 1 in the same block");
    }

    // --- a patch taps the bus OUTPUT, after its fader, mute and limiter ---
    //
    // 7.1 puts the output patch after the safety limiter, so the source bus's own
    // processing must already have happened when a patch reads it. That is the
    // real reason the schedule has to be in dependency order, and testing only
    // "signal is present" does not catch getting it wrong: strips accumulate into
    // the bus buffer before the bus is processed, so a naive strip-then-bus order
    // still finds the raw sum sitting there. These two checks distinguish them.
    {
        Graph g;
        g.configure({kRate, kBlock, 2, 2});
        g.strip(0).set_send_enabled(0, true);
        g.strip(1).set_send_enabled(1, true);
        OMNI_CHECK(g.add_patch(bus_out(0, 0), strip_in(1, 0), 0.0f, why) ==
                   PatchResult::Ok);
        g.bus(0).set_fader_db(-20.0f);   // must be applied BEFORE the patch reads it
        g.snap();
        run_blocks(g, tone, 0, 30);

        const float at_source = peak(g.bus_output(0, 0), kBlock);
        const float at_dest = peak(g.bus_output(1, 0), kBlock);
        char msg[200];
        std::snprintf(msg, sizeof msg,
                      "patch must carry the bus OUTPUT: source %.5f, dest %.5f "
                      "(expected about equal, not 10x)",
                      static_cast<double>(at_source), static_cast<double>(at_dest));
        OMNI_CHECK_MSG(std::fabs(at_dest - at_source) < 0.02f * at_source + 1e-5f, msg);
    }
    // --- and a muted source bus patches exact silence --------------------
    {
        Graph g;
        g.configure({kRate, kBlock, 2, 2});
        g.strip(0).set_send_enabled(0, true);
        g.strip(1).set_send_enabled(1, true);
        OMNI_CHECK(g.add_patch(bus_out(0, 0), strip_in(1, 0), 0.0f, why) ==
                   PatchResult::Ok);
        g.bus(0).set_mute(true);
        g.snap();
        run_blocks(g, tone, 0, 30);
        check_exact_silence(g, 1, kBlock,
                            "a patch from a MUTED bus must carry exact silence");
    }

    // --- QS-04: MANY-TO-ONE ----------------------------------------------
    // Two buses patched into one strip must sum, and every unrelated bus must
    // stay bit-exact silent.
    {
        Graph g;
        g.configure({kRate, kBlock, 4, 4});
        g.strip(0).set_send_enabled(0, true);
        g.strip(1).set_send_enabled(1, true);
        g.strip(3).set_send_enabled(3, true);
        OMNI_CHECK(g.add_patch(bus_out(0, 0), strip_in(3, 0), 0.0f, why) == PatchResult::Ok);
        OMNI_CHECK(g.add_patch(bus_out(1, 0), strip_in(3, 0), 0.0f, why) == PatchResult::Ok);
        g.snap();

        // Drive strips 0 and 1; strip 2 stays silent and unrouted.
        for (int i = 0; i < 20; ++i) {
            g.clear_inputs();
            for (std::size_t s : {std::size_t{0}, std::size_t{1}}) {
                std::copy(tone.begin(), tone.end(), g.strip_input(s, 0));
                std::copy(tone.begin(), tone.end(), g.strip_input(s, 1));
            }
            g.process(kBlock);
        }
        const float one = peak(g.bus_output(0, 0), kBlock);
        const float summed = peak(g.bus_output(3, 0), kBlock);
        OMNI_CHECK(one > 0.1f);
        // Two identical sources sum to twice the amplitude.
        char msg[160];
        std::snprintf(msg, sizeof msg, "many-to-one: %.4f should be about 2x %.4f",
                      static_cast<double>(summed), static_cast<double>(one));
        OMNI_CHECK_MSG(std::fabs(summed - 2.0f * one) < 0.01f * one + 1e-4f, msg);
        check_exact_silence(g, 2, kBlock, "unrouted bus beside a many-to-one patch");
    }

    // --- QS-04: ONE-TO-MANY ----------------------------------------------
    {
        Graph g;
        g.configure({kRate, kBlock, 4, 4});
        g.strip(0).set_send_enabled(0, true);
        g.strip(1).set_send_enabled(1, true);
        g.strip(2).set_send_enabled(2, true);
        OMNI_CHECK(g.add_patch(bus_out(0, 0), strip_in(1, 0), 0.0f, why) == PatchResult::Ok);
        OMNI_CHECK(g.add_patch(bus_out(0, 0), strip_in(2, 0), 0.0f, why) == PatchResult::Ok);
        g.snap();
        run_blocks(g, tone, 0, 20);
        OMNI_CHECK(peak(g.bus_output(1, 0), kBlock) > 0.1f);
        OMNI_CHECK(peak(g.bus_output(2, 0), kBlock) > 0.1f);
        // Bus 3 has no send and no patch: exactly silent, with one-to-many live.
        check_exact_silence(g, 3, kBlock, "unrouted bus beside a one-to-many patch");
        // And the right-hand channel of strips 1 and 2 was never patched, so bus 1
        // and 2's right legs carry nothing.
        std::size_t right_nonzero = 0;
        for (std::size_t b : {std::size_t{1}, std::size_t{2}}) {
            const float* p = g.bus_output(b, 1);
            for (std::size_t i = 0; i < kBlock; ++i)
                if (p[i] != 0.0f) ++right_nonzero;
        }
        OMNI_CHECK_MSG(right_nonzero == 0,
                       "an unpatched channel must stay exactly silent (QS-04)");
    }

    // --- patch changes crossfade rather than step (QS-07) ----------------
    {
        Graph g;
        g.configure({kRate, kBlock, 2, 2});
        g.strip(0).set_send_enabled(0, true);
        g.strip(1).set_send_enabled(1, true);
        g.snap();

        // Add a patch while audio runs, and watch bus 1's envelope rise smoothly.
        OMNI_CHECK(g.add_patch(bus_out(0, 0), strip_in(1, 0), 0.0f, why) == PatchResult::Ok);
        std::vector<float> peaks;
        for (int i = 0; i < 40; ++i) {
            g.clear_inputs();
            std::copy(tone.begin(), tone.end(), g.strip_input(0, 0));
            std::copy(tone.begin(), tone.end(), g.strip_input(0, 1));
            g.process(kBlock);
            peaks.push_back(peak(g.bus_output(1, 0), kBlock));
        }
        // The first block must not already be at full level: that would be a step.
        OMNI_CHECK_MSG(peaks[0] < peaks.back() * 0.9f,
                       "a new patch must fade in, not step in");
        OMNI_CHECK_MSG(peaks.back() > 0.1f, "the patch should reach full level");
        // Monotonic rise, no overshoot.
        std::size_t dips = 0;
        for (std::size_t i = 1; i < 10; ++i)
            if (peaks[i] + 1e-6f < peaks[i - 1]) ++dips;
        OMNI_CHECK_MSG(dips == 0, "the fade-in should not dip");

        // Removing fades out and then prunes.
        OMNI_CHECK(g.remove_patch(bus_out(0, 0), strip_in(1, 0)));
        OMNI_CHECK_MSG(g.prune_patches() == 0,
                       "a patch must not be erased before it has faded");
        for (int i = 0; i < 40; ++i) {
            g.clear_inputs();
            std::copy(tone.begin(), tone.end(), g.strip_input(0, 0));
            std::copy(tone.begin(), tone.end(), g.strip_input(0, 1));
            g.process(kBlock);
        }
        check_exact_silence(g, 1, kBlock, "after a patch is removed");
        OMNI_CHECK_MSG(g.prune_patches() == 1, "a faded patch should be erased");
        OMNI_CHECK(g.patch_bay().entries().empty());
    }

    // --- compile() reports a cycle if one is ever constructed ------------
    // add_patch prevents this, so reaching it means something bypassed the guard.
    // The assertion is that compile() detects rather than hangs or corrupts.
    {
        Graph g;
        g.configure({kRate, kBlock, 1, 1});
        OMNI_CHECK(g.add_patch(bus_out(0, 0), strip_in(0, 0), 0.0f, why) == PatchResult::Ok);
        // Now enable the send that closes it, bypassing send_would_loop.
        OMNI_CHECK(g.send_would_loop(0, 0));
        g.strip(0).set_send_enabled(0, true);
        std::string compile_why;
        OMNI_CHECK_MSG(!g.compile(compile_why), "compile() must detect the cycle");
        OMNI_CHECK(!compile_why.empty());
    }

    return finish();
}
