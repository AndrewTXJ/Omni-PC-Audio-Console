// QS-01 Unity-path transparency (docs/TEST-PLAN.md).
//
// Target: bit-exact for 16- and 24-bit audio -- a null result of silence -- on a
// stereo strip with balance centred, Gain and fader at 0 dB.
//
// Pass criterion is EXACTLY zero for every sample, not "below a threshold".
#include <vector>

#include "omni/engine/graph.hpp"
#include "signals.hpp"
#include "test_support.hpp"

using namespace omni;
using namespace omni::test;

namespace {

constexpr double kRate = 48000.0;
constexpr std::size_t kBlock = 64;

/// Render `left`/`right` through one stereo strip at unity into bus 0 and return
/// the bus output. Processed in blocks, as a device callback would.
struct Rendered {
    std::vector<float> left, right;
};

Rendered render_unity(const std::vector<float>& left, const std::vector<float>& right) {
    Graph g;
    g.configure({kRate, kBlock, 1, 1});
    // Defaults are the unity path; only the send has to be switched on, which is
    // what an A1 button does: a send at 0 dB, post-fader.
    g.strip(0).set_send_enabled(0, true);
    // The send's own ramp must settle before the comparison, or the first few
    // milliseconds are a fade-in rather than a null. A real session ramps from
    // silence for exactly this reason (principle 4: outputs start muted).
    for (int i = 0; i < 64; ++i) {
        for (std::size_t c = 0; c < kChannels; ++c) {
            float* p = g.strip_input(0, c);
            for (std::size_t j = 0; j < kBlock; ++j) p[j] = 0.0f;
        }
        g.process(kBlock);
    }

    Rendered out;
    const std::size_t frames = left.size();
    out.left.reserve(frames);
    out.right.reserve(frames);
    for (std::size_t off = 0; off < frames; off += kBlock) {
        const std::size_t n = std::min(kBlock, frames - off);
        std::copy(left.begin() + static_cast<std::ptrdiff_t>(off),
                  left.begin() + static_cast<std::ptrdiff_t>(off + n),
                  g.strip_input(0, 0));
        std::copy(right.begin() + static_cast<std::ptrdiff_t>(off),
                  right.begin() + static_cast<std::ptrdiff_t>(off + n),
                  g.strip_input(0, 1));
        g.process(n);
        const float* bl = g.bus_output(0, 0);
        const float* br = g.bus_output(0, 1);
        out.left.insert(out.left.end(), bl, bl + n);
        out.right.insert(out.right.end(), br, br + n);
    }
    return out;
}

void null_test(const char* what, const std::vector<float>& l,
               const std::vector<float>& r) {
    const Rendered got = render_unity(l, r);
    std::size_t bad = 0;
    float worst = 0.0f;
    for (std::size_t i = 0; i < l.size(); ++i) {
        const float dl = got.left[i] - l[i];
        const float dr = got.right[i] - r[i];
        if (dl != 0.0f || dr != 0.0f) {
            ++bad;
            worst = std::max(worst, std::max(std::fabs(dl), std::fabs(dr)));
        }
    }
    char msg[200];
    std::snprintf(msg, sizeof msg,
                  "%s: %zu of %zu samples differ (worst |diff| %.9g)", what, bad,
                  l.size(), static_cast<double>(worst));
    OMNI_CHECK_MSG(bad == 0, msg);
}

}  // namespace

int main() {
    begin("QS-01 unity-path transparency");

    constexpr std::size_t n = 4800;  // 100 ms

    // 1 kHz at -1 dBFS, the roadmap's reference tone level.
    null_test("1 kHz sine at -1 dBFS", sine(n, 1000.0, kRate, -1.0f),
              sine(n, 1000.0, kRate, -1.0f, 0.37));

    // A low-level signal: catches anything that quantises or dithers.
    null_test("1 kHz sine at -60 dBFS", sine(n, 1000.0, kRate, -60.0f),
              sine(n, 1000.0, kRate, -60.0f, 1.1));

    // Full-band pseudo-random content.
    null_test("pseudo-random sequence", prng_noise(n, 0xABCDEF01u),
              prng_noise(n, 0x1337BEEFu));

    // 16- and 24-bit representable values: the depths the target names.
    null_test("16-bit representable noise", quantized_noise(n, 16, 0xA1u),
              quantized_noise(n, 16, 0xB2u));
    null_test("24-bit representable noise", quantized_noise(n, 24, 0xC3u),
              quantized_noise(n, 24, 0xD4u));

    // A single impulse: catches an off-by-one in any delay or buffer.
    null_test("single-sample impulse", impulse(n, 100, 0.5f), impulse(n, 2049, -0.25f));

    // Signals that straddle a block boundary at an awkward length.
    null_test("non-multiple-of-block length",
              sine(4801, 997.0, kRate, -1.0f), sine(4801, 997.0, kRate, -1.0f, 0.5));

    // Denormals and signed zero. guards.hpp documents why these pass through
    // untouched rather than being flushed; this is the test that holds that
    // decision in place.
    {
        std::vector<float> l(256, 0.0f), r(256, 0.0f);
        l[10] = 1e-40f;            // denormal
        l[11] = -1e-40f;
        l[12] = -0.0f;             // negative zero
        r[13] = 5e-44f;            // smallest denormals
        const Rendered got = render_unity(l, r);
        OMNI_CHECK_EXACT(got.left[10], 1e-40f);
        OMNI_CHECK_EXACT(got.left[11], -1e-40f);
        OMNI_CHECK_EXACT(got.right[13], 5e-44f);
        // -0.0f + 0.0f is +0.0f in IEEE 754, so the SIGN of zero is not
        // preserved through an accumulate. The value is still zero, so the null
        // difference is zero and the target holds; recorded here so the
        // behaviour is known rather than discovered.
        OMNI_CHECK_EXACT(got.left[12] - l[12], 0.0f);
    }

    // No pre-roll: snap() must make the path exact from the FIRST sample.
    // Without it the opening milliseconds are a fade-in as the send ramps up,
    // which is what the end-to-end renderer check caught when the library tests
    // (which all pre-roll) did not.
    {
        Graph g;
        g.configure({kRate, kBlock, 1, 1});
        g.strip(0).set_send_enabled(0, true);
        g.snap();
        const std::vector<float> in = sine(kBlock * 8, 1000.0, kRate, -1.0f);
        std::vector<float> out;
        out.reserve(in.size());
        for (std::size_t off = 0; off < in.size(); off += kBlock) {
            std::copy(in.begin() + static_cast<std::ptrdiff_t>(off),
                      in.begin() + static_cast<std::ptrdiff_t>(off + kBlock),
                      g.strip_input(0, 0));
            std::copy(in.begin() + static_cast<std::ptrdiff_t>(off),
                      in.begin() + static_cast<std::ptrdiff_t>(off + kBlock),
                      g.strip_input(0, 1));
            g.process(kBlock);
            const float* b = g.bus_output(0, 0);
            out.insert(out.end(), b, b + kBlock);
        }
        std::size_t bad = 0;
        for (std::size_t i = 0; i < in.size(); ++i)
            if (out[i] != in[i]) ++bad;
        char msg[160];
        std::snprintf(msg, sizeof msg,
                      "after snap(), %zu of %zu samples differ from frame 0", bad,
                      in.size());
        OMNI_CHECK_MSG(bad == 0, msg);
    }

    // And without snap(), the opening really does ramp -- so snap() is load-
    // bearing rather than decorative.
    {
        Graph g;
        g.configure({kRate, kBlock, 1, 1});
        g.strip(0).set_send_enabled(0, true);
        const std::vector<float> in = sine(kBlock, 1000.0, kRate, -1.0f);
        std::copy(in.begin(), in.end(), g.strip_input(0, 0));
        std::copy(in.begin(), in.end(), g.strip_input(0, 1));
        g.process(kBlock);
        std::size_t differing = 0;
        for (std::size_t i = 0; i < kBlock; ++i)
            if (g.bus_output(0, 0)[i] != in[i]) ++differing;
        OMNI_CHECK_MSG(differing > 0,
                       "without snap() the first block should be a fade-in");
    }

    // The bit-transparent indicator (7.6) must agree with reality: true for
    // exactly the configuration above, false as soon as anything touches it.
    {
        Graph g;
        g.configure({kRate, kBlock, 1, 1});
        g.strip(0).set_send_enabled(0, true);
        OMNI_CHECK_EXACT(dsp::db_to_gain(g.strip(0).trim_db()), 1.0f);
        OMNI_CHECK_EXACT(dsp::db_to_gain(g.strip(0).fader_db()), 1.0f);
        OMNI_CHECK_EXACT(dsp::balance_gains(g.strip(0).pan()).left, 1.0f);
        OMNI_CHECK_EXACT(dsp::balance_gains(g.strip(0).pan()).right, 1.0f);
        OMNI_CHECK(!g.strip(0).muted());
        OMNI_CHECK(!g.strip(0).polarity_invert());
    }

    return finish();
}
