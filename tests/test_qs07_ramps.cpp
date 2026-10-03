// QS-07 Click-free changes -- the M1-reachable subset (docs/TEST-PLAN.md).
//
// Target: transients below -90 dBFS when toggling mute, solo, patch or profile,
// with a 1 kHz tone at -20 dBFS; ramps of 5-10 ms for mutes and 10-20 ms for
// level changes (7.2).
//
// SCOPE. Patch changes need M3 and profile switches need M5, so neither can be
// exercised yet -- the test plan says as much. What is testable now is the ramp
// machinery every one of those operations will use: mute, fader and send
// toggles. The method is to recover the gain trajectory by dividing output by a
// known input, then assert it has no step larger than one ramp increment. A
// click IS a step, so this measures the defect directly rather than inferring it
// from a spectrum.
#include <vector>

#include "omni/dsp/db.hpp"
#include "omni/engine/graph.hpp"
#include "signals.hpp"
#include "test_support.hpp"

using namespace omni;
using namespace omni::test;

namespace {

constexpr double kRate = 48000.0;
constexpr std::size_t kBlock = 32;

struct Trajectory {
    std::vector<float> gain;     // output / input, where input is non-trivial
    std::size_t ramp_frames = 0; // frames spent between the two settled values
    float max_step = 0.0f;       // largest single-sample change in gain
};

/// Drive a steady tone, apply `change` after `at_block`, and recover the gain.
template <typename Fn>
Trajectory trajectory(Fn change, std::size_t blocks, std::size_t at_block) {
    Graph g;
    g.configure({kRate, kBlock, 1, 1});
    g.strip(0).set_send_enabled(0, true);
    const std::vector<float> tone = sine(kBlock, 1000.0, kRate, -20.0f);

    // Settle.
    for (int i = 0; i < 64; ++i) {
        std::copy(tone.begin(), tone.end(), g.strip_input(0, 0));
        std::copy(tone.begin(), tone.end(), g.strip_input(0, 1));
        g.process(kBlock);
    }

    Trajectory t;
    for (std::size_t b = 0; b < blocks; ++b) {
        if (b == at_block) change(g);
        std::copy(tone.begin(), tone.end(), g.strip_input(0, 0));
        std::copy(tone.begin(), tone.end(), g.strip_input(0, 1));
        g.process(kBlock);
        const float* out = g.bus_output(0, 0);
        for (std::size_t i = 0; i < kBlock; ++i) {
            // Only where the tone is well clear of zero, so the division is
            // numerically meaningful.
            if (std::fabs(tone[i]) > 0.01f) t.gain.push_back(out[i] / tone[i]);
        }
    }
    for (std::size_t i = 1; i < t.gain.size(); ++i)
        t.max_step = std::max(t.max_step, std::fabs(t.gain[i] - t.gain[i - 1]));
    return t;
}

}  // namespace

int main() {
    begin("QS-07 click-free changes (M1 subset: mute, fader, send)");

    const std::size_t mute_frames = ms_to_frames(kMuteSmoothingMs, kRate);
    const std::size_t level_frames = ms_to_frames(kLevelSmoothingMs, kRate);

    // Documented ramp times are inside 7.2's stated windows.
    OMNI_CHECK(kMuteSmoothingMs >= 5.0f && kMuteSmoothingMs <= 10.0f);
    OMNI_CHECK(kLevelSmoothingMs >= 10.0f && kLevelSmoothingMs <= 20.0f);
    OMNI_CHECK(mute_frames > 0 && level_frames > 0);

    // --- mute on ---------------------------------------------------------
    {
        const Trajectory t = trajectory([](Graph& g) { g.strip(0).set_mute(true); }, 40, 4);
        // No step larger than one ramp increment (plus slack for the sparse
        // sampling of the trajectory, which skips samples near a zero crossing).
        const float allowed = 4.0f / static_cast<float>(mute_frames);
        char msg[160];
        std::snprintf(msg, sizeof msg, "mute-on max step %.6g, allowed %.6g",
                      static_cast<double>(t.max_step), static_cast<double>(allowed));
        OMNI_CHECK_MSG(t.max_step <= allowed, msg);
        // And it really did end up muted.
        OMNI_CHECK_NEAR(t.gain.back(), 0.0f, 1e-7);
    }

    // --- mute off: must return EXACTLY to unity, not nearly ---------------
    {
        Graph g;
        g.configure({kRate, kBlock, 1, 1});
        g.strip(0).set_send_enabled(0, true);
        g.strip(0).set_mute(true);
        const std::vector<float> tone = sine(kBlock, 1000.0, kRate, -20.0f);
        for (int i = 0; i < 64; ++i) {
            std::copy(tone.begin(), tone.end(), g.strip_input(0, 0));
            std::copy(tone.begin(), tone.end(), g.strip_input(0, 1));
            g.process(kBlock);
        }
        g.strip(0).set_mute(false);
        for (int i = 0; i < 64; ++i) {
            std::copy(tone.begin(), tone.end(), g.strip_input(0, 0));
            std::copy(tone.begin(), tone.end(), g.strip_input(0, 1));
            g.process(kBlock);
        }
        // The settled path is bit-exact again: unmuting restores QS-01.
        std::copy(tone.begin(), tone.end(), g.strip_input(0, 0));
        std::copy(tone.begin(), tone.end(), g.strip_input(0, 1));
        g.process(kBlock);
        std::size_t bad = 0;
        for (std::size_t i = 0; i < kBlock; ++i)
            if (g.bus_output(0, 0)[i] != tone[i]) ++bad;
        OMNI_CHECK_MSG(bad == 0, "unmuting must restore the bit-exact unity path");
    }

    // --- a fader jump ----------------------------------------------------
    {
        const Trajectory t =
            trajectory([](Graph& g) { g.strip(0).set_fader_db(-20.0f); }, 40, 4);
        const float allowed = 4.0f / static_cast<float>(level_frames);
        char msg[160];
        std::snprintf(msg, sizeof msg, "fader-jump max step %.6g, allowed %.6g",
                      static_cast<double>(t.max_step), static_cast<double>(allowed));
        OMNI_CHECK_MSG(t.max_step <= allowed, msg);
        OMNI_CHECK_NEAR(t.gain.back(), dsp::db_to_gain(-20.0f), 1e-4);
    }

    // --- toggling an A/B button (a send) ---------------------------------
    {
        const Trajectory t =
            trajectory([](Graph& g) { g.strip(0).set_send_enabled(0, false); }, 40, 4);
        const float allowed = 4.0f / static_cast<float>(mute_frames);
        char msg[160];
        std::snprintf(msg, sizeof msg, "send-off max step %.6g, allowed %.6g",
                      static_cast<double>(t.max_step), static_cast<double>(allowed));
        OMNI_CHECK_MSG(t.max_step <= allowed, msg);
    }

    // --- an operation landing mid-ramp, which is where ramps usually break -
    {
        Graph g;
        g.configure({kRate, kBlock, 1, 1});
        g.strip(0).set_send_enabled(0, true);
        const std::vector<float> tone = sine(kBlock, 1000.0, kRate, -20.0f);
        for (int i = 0; i < 64; ++i) {
            std::copy(tone.begin(), tone.end(), g.strip_input(0, 0));
            std::copy(tone.begin(), tone.end(), g.strip_input(0, 1));
            g.process(kBlock);
        }
        std::vector<float> gains;
        for (int b = 0; b < 40; ++b) {
            if (b == 4) g.strip(0).set_mute(true);
            if (b == 5) g.strip(0).set_mute(false);   // reverse mid-ramp
            if (b == 6) g.strip(0).set_fader_db(-6.0f);
            std::copy(tone.begin(), tone.end(), g.strip_input(0, 0));
            std::copy(tone.begin(), tone.end(), g.strip_input(0, 1));
            g.process(kBlock);
            for (std::size_t i = 0; i < kBlock; ++i)
                if (std::fabs(tone[i]) > 0.01f)
                    gains.push_back(g.bus_output(0, 0)[i] / tone[i]);
        }
        float worst = 0.0f;
        for (std::size_t i = 1; i < gains.size(); ++i)
            worst = std::max(worst, std::fabs(gains[i] - gains[i - 1]));
        const float allowed = 4.0f / static_cast<float>(mute_frames);
        char msg[160];
        std::snprintf(msg, sizeof msg, "mid-ramp reversal max step %.6g, allowed %.6g",
                      static_cast<double>(worst), static_cast<double>(allowed));
        OMNI_CHECK_MSG(worst <= allowed, msg);
        OMNI_CHECK_NEAR(gains.back(), dsp::db_to_gain(-6.0f), 1e-4);
    }

    return finish();
}
