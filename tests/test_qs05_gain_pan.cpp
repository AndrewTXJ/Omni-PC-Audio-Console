// QS-05 Gain and pan accuracy (docs/TEST-PLAN.md).
//
// Target: within 0.01 dB for gain, 0.05 dB for the selected pan law, with 0 dB
// and centred balance EXACT rather than merely within tolerance -- QS-01 depends
// on that exactness.
#include <vector>

#include "omni/dsp/db.hpp"
#include "omni/dsp/pan.hpp"
#include "omni/engine/graph.hpp"
#include "signals.hpp"
#include "test_support.hpp"

using namespace omni;
using namespace omni::test;

namespace {

constexpr double kRate = 48000.0;
constexpr std::size_t kBlock = 64;

/// Measured gain, in dB, of a settled strip driven by a steady tone.
double measured_gain_db(float trim_db, float fader_db) {
    Graph g;
    g.configure({kRate, kBlock, 1, 1});
    g.strip(0).set_send_enabled(0, true);
    g.strip(0).set_trim_db(trim_db);
    g.strip(0).set_fader_db(fader_db);

    // -50 dBFS, not -20: trim can reach +24 dB and the fader +12 dB, so a
    // louder tone would exceed full scale and the bus safety limiter would
    // (correctly) clamp it -- measuring the limiter instead of the gain. The
    // first run of this test did exactly that and read 21.47 dB for +24 dB.
    const std::vector<float> tone = sine(kBlock, 1000.0, kRate, -50.0f);
    // Let every ramp settle before measuring.
    for (int i = 0; i < 64; ++i) {
        std::copy(tone.begin(), tone.end(), g.strip_input(0, 0));
        std::copy(tone.begin(), tone.end(), g.strip_input(0, 1));
        g.process(kBlock);
    }
    // One more block, measured.
    std::copy(tone.begin(), tone.end(), g.strip_input(0, 0));
    std::copy(tone.begin(), tone.end(), g.strip_input(0, 1));
    g.process(kBlock);

    const double in_rms = rms(tone.data(), kBlock);
    const double out_rms = rms(g.bus_output(0, 0), kBlock);
    if (in_rms == 0.0 || out_rms == 0.0) return -1000.0;
    return 20.0 * std::log10(out_rms / in_rms);
}

}  // namespace

int main() {
    begin("QS-05 gain and pan accuracy");

    // --- dB <-> linear round trip, 0.01 dB ------------------------------
    for (int tenths = -240; tenths <= 240; ++tenths) {   // +/-24 dB in 0.1 dB steps
        const float db = static_cast<float>(tenths) * 0.1f;
        const float back = dsp::gain_to_db(dsp::db_to_gain(db));
        OMNI_CHECK_NEAR(back, db, 0.01);
    }
    for (int tenths = -600; tenths <= 120; ++tenths) {   // fader range
        const float db = static_cast<float>(tenths) * 0.1f;
        OMNI_CHECK_NEAR(dsp::gain_to_db(dsp::db_to_gain(db)), db, 0.01);
    }

    // 0 dB is exactly unity, both directions. 7.2 also requires a reset to land
    // exactly on 0 dB, which is the same assertion.
    OMNI_CHECK_EXACT(dsp::db_to_gain(0.0f), 1.0f);
    OMNI_CHECK_EXACT(dsp::gain_to_db(1.0f), 0.0f);
    // The -inf end of the fader is exactly silent, not very quiet.
    OMNI_CHECK_EXACT(dsp::db_to_gain(dsp::kSilenceDb), 0.0f);
    OMNI_CHECK_EXACT(dsp::db_to_gain(-200.0f), 0.0f);

    // --- measured gain through the engine, 0.01 dB -----------------------
    for (float trim : {-24.0f, -12.0f, -6.0f, -0.1f, 0.0f, 0.1f, 6.0f, 12.0f, 24.0f}) {
        const double got = measured_gain_db(trim, 0.0f);
        OMNI_CHECK_NEAR(got, static_cast<double>(trim), 0.01);
    }
    for (float fader : {-40.0f, -20.0f, -6.0f, 0.0f, 6.0f, 12.0f}) {
        const double got = measured_gain_db(0.0f, fader);
        OMNI_CHECK_NEAR(got, static_cast<double>(fader), 0.01);
    }
    // Trim and fader compose.
    OMNI_CHECK_NEAR(measured_gain_db(-6.0f, -6.0f), -12.0, 0.01);
    OMNI_CHECK_NEAR(measured_gain_db(12.0f, -12.0f), 0.0, 0.01);

    // Ranges are clamped to 7.2's limits rather than silently exceeded.
    {
        Graph g;
        g.configure({kRate, kBlock, 1, 1});
        g.strip(0).set_trim_db(100.0f);
        OMNI_CHECK_EXACT(g.strip(0).trim_db(), kTrimMaxDb);
        g.strip(0).set_trim_db(-100.0f);
        OMNI_CHECK_EXACT(g.strip(0).trim_db(), kTrimMinDb);
        g.strip(0).set_fader_db(99.0f);
        OMNI_CHECK_EXACT(g.strip(0).fader_db(), kFaderMaxDb);
    }

    // --- the limiter is genuinely in the path ----------------------------
    // Gain that pushes past full scale must be clamped, not passed. This is the
    // behaviour that made the first version of the gain sweep above read 21.47 dB
    // instead of 24, so it is worth asserting rather than merely avoiding.
    {
        Graph g;
        g.configure({kRate, kBlock, 1, 1});
        g.strip(0).set_send_enabled(0, true);
        g.strip(0).set_trim_db(24.0f);
        const std::vector<float> loud = sine(kBlock, 1000.0, kRate, -6.0f);
        for (int i = 0; i < 64; ++i) {
            std::copy(loud.begin(), loud.end(), g.strip_input(0, 0));
            std::copy(loud.begin(), loud.end(), g.strip_input(0, 1));
            g.process(kBlock);
        }
        std::copy(loud.begin(), loud.end(), g.strip_input(0, 0));
        std::copy(loud.begin(), loud.end(), g.strip_input(0, 1));
        g.process(kBlock);
        OMNI_CHECK(g.bus(0).limiter().engaged());
        OMNI_CHECK(peak(g.bus_output(0, 0), kBlock) <= g.bus(0).limiter().ceiling());
    }

    // --- pan law centre attenuation, 0.05 dB ----------------------------
    for (auto law : {dsp::PanLaw::ZeroDb, dsp::PanLaw::Minus3Db,
                     dsp::PanLaw::Minus4_5Db, dsp::PanLaw::Minus6Db}) {
        const dsp::StereoGain c = dsp::pan_gains(0.0f, law);
        const float want = dsp::pan_law_centre_db(law);
        OMNI_CHECK_NEAR(dsp::gain_to_db(c.left), want, 0.05);
        OMNI_CHECK_NEAR(dsp::gain_to_db(c.right), want, 0.05);
        // Symmetric at centre.
        OMNI_CHECK_NEAR(c.left, c.right, 1e-6);

        // Hard left and hard right: the near leg is exactly unity, the far leg
        // exactly silent. Anything else leaks a panned source into the far side.
        const dsp::StereoGain l = dsp::pan_gains(-1.0f, law);
        const dsp::StereoGain r = dsp::pan_gains(1.0f, law);
        OMNI_CHECK_EXACT(l.left, 1.0f);
        OMNI_CHECK_EXACT(l.right, 0.0f);
        OMNI_CHECK_EXACT(r.left, 0.0f);
        OMNI_CHECK_EXACT(r.right, 1.0f);

        // Monotonic across the sweep: left falls, right rises.
        float prev_l = 2.0f, prev_r = -1.0f;
        for (int i = -100; i <= 100; ++i) {
            const dsp::StereoGain g = dsp::pan_gains(static_cast<float>(i) * 0.01f, law);
            OMNI_CHECK(g.left <= prev_l + 1e-6f);
            OMNI_CHECK(g.right >= prev_r - 1e-6f);
            prev_l = g.left;
            prev_r = g.right;
        }
    }

    // --- balance: centred is EXACTLY unity (what QS-01 rests on) ---------
    {
        const dsp::StereoGain c = dsp::balance_gains(0.0f);
        OMNI_CHECK_EXACT(c.left, 1.0f);
        OMNI_CHECK_EXACT(c.right, 1.0f);
        // Turning right leaves the right leg untouched and lowers the left.
        const dsp::StereoGain r = dsp::balance_gains(0.5f);
        OMNI_CHECK_EXACT(r.right, 1.0f);
        OMNI_CHECK_NEAR(r.left, 0.5f, 1e-6);
        const dsp::StereoGain hard = dsp::balance_gains(1.0f);
        OMNI_CHECK_EXACT(hard.left, 0.0f);
        OMNI_CHECK_EXACT(hard.right, 1.0f);
    }

    return finish();
}
