// Unit tests for the pieces the quality specifications depend on: saturating
// conversion (4.1), the safety limiter (4.3), metering (4.3, 7.4), the delay
// line (7.2) and WAV round-tripping (4.1's offline renderer).
#include <vector>

#include "omni/dsp/guards.hpp"
#include "omni/dsp/limiter.hpp"
#include "omni/dsp/meter.hpp"
#include "omni/dsp/ramp.hpp"
#include "omni/engine/delay.hpp"
#include "omni/io/wav.hpp"
#include "signals.hpp"
#include "test_support.hpp"

using namespace omni;
using namespace omni::test;

int main() {
    begin("DSP units");

    // --- float -> int saturates and never wraps (4.1) --------------------
    // A wrap turns a mild overshoot into full-scale noise, which is a
    // hearing-safety matter. These are the values that would wrap.
    OMNI_CHECK(dsp::to_i16(2.0f) == 32767);
    OMNI_CHECK(dsp::to_i16(-2.0f) == -32768);
    OMNI_CHECK(dsp::to_i16(1.0f) == 32767);
    OMNI_CHECK(dsp::to_i16(-1.0f) == -32768);
    OMNI_CHECK(dsp::to_i16(1e30f) == 32767);
    OMNI_CHECK(dsp::to_i24(2.0f) == 8388607);
    OMNI_CHECK(dsp::to_i24(-2.0f) == -8388608);
    OMNI_CHECK(dsp::to_i32(2.0f) == 2147483647);
    OMNI_CHECK(dsp::to_i32(-2.0f) == -2147483648);
    // Non-finite input becomes silence, not a wrapped extreme.
    const float nan_v = std::nanf("");
    const float inf_v = std::numeric_limits<float>::infinity();
    OMNI_CHECK(dsp::to_i16(nan_v) == 0);
    OMNI_CHECK(dsp::to_i16(inf_v) == 0);
    OMNI_CHECK(dsp::to_i24(-inf_v) == 0);
    OMNI_CHECK(dsp::to_i32(nan_v) == 0);
    // Round trip at 16 and 24 bits is exact for representable values.
    for (int v = -32768; v <= 32767; v += 997)
        OMNI_CHECK(dsp::to_i16(dsp::from_i16(static_cast<std::int16_t>(v))) == v);

    // --- sanitize is the identity for valid audio ------------------------
    OMNI_CHECK_EXACT(dsp::sanitize(0.5f), 0.5f);
    OMNI_CHECK_EXACT(dsp::sanitize(-0.0f), -0.0f);
    OMNI_CHECK_EXACT(dsp::sanitize(1e-40f), 1e-40f);   // denormals survive
    OMNI_CHECK_EXACT(dsp::sanitize(nan_v), 0.0f);
    OMNI_CHECK_EXACT(dsp::sanitize(inf_v), 0.0f);
    // kill_denormal is for recursive state, and does what it says.
    OMNI_CHECK_EXACT(dsp::kill_denormal(1e-40f), 0.0f);
    OMNI_CHECK_EXACT(dsp::kill_denormal(0.5f), 0.5f);

    // --- safety limiter: transparent below the ceiling -------------------
    {
        dsp::SafetyLimiter lim;
        std::vector<float> buf = prng_noise(512, 0x5EEDu);
        for (auto& v : buf) v *= 0.5f;          // well under full scale
        const std::vector<float> before = buf;
        lim.process(buf.data(), buf.size());
        std::size_t changed = 0;
        for (std::size_t i = 0; i < buf.size(); ++i)
            if (buf[i] != before[i]) ++changed;
        OMNI_CHECK_MSG(changed == 0, "limiter must be bit-transparent below ceiling");
        OMNI_CHECK(!lim.engaged());
    }
    // --- and clamps above it --------------------------------------------
    {
        dsp::SafetyLimiter lim;
        lim.set_ceiling(0.5f);
        std::vector<float> buf{0.9f, -0.9f, 0.1f, nan_v, inf_v};
        lim.process(buf.data(), buf.size());
        OMNI_CHECK_EXACT(buf[0], 0.5f);
        OMNI_CHECK_EXACT(buf[1], -0.5f);
        OMNI_CHECK_EXACT(buf[2], 0.1f);
        OMNI_CHECK_EXACT(buf[3], 0.0f);   // NaN sanitised, not clamped
        OMNI_CHECK_EXACT(buf[4], 0.0f);   // Inf likewise
        OMNI_CHECK(lim.engaged());
    }

    // --- meter: peak hold and latched clip ------------------------------
    {
        dsp::PeakMeter m;
        std::vector<float> buf{0.25f, -0.5f, 0.1f};
        m.process(buf.data(), buf.size());
        OMNI_CHECK_NEAR(m.peek_peak(), 0.5f, 1e-7);
        OMNI_CHECK(!m.clipped());
        // Peak HOLDS across blocks until read.
        std::vector<float> quiet{0.01f};
        m.process(quiet.data(), quiet.size());
        OMNI_CHECK_NEAR(m.peek_peak(), 0.5f, 1e-7);
        // Reading resets the hold.
        OMNI_CHECK_NEAR(m.read_peak(), 0.5f, 1e-7);
        OMNI_CHECK_NEAR(m.peek_peak(), 0.0f, 1e-7);
        // Clip latches and survives until cleared (4.3's latched clip LED).
        std::vector<float> hot{1.5f};
        m.process(hot.data(), hot.size());
        OMNI_CHECK(m.clipped());
        (void)m.read_peak();
        OMNI_CHECK(m.clipped());
        m.clear_clip();
        OMNI_CHECK(!m.clipped());
    }

    // --- ramp lands exactly on target -----------------------------------
    {
        dsp::Ramp r(0.0f);
        r.set_target(1.0f, 100);
        for (int i = 0; i < 100; ++i) r.next();
        OMNI_CHECK_EXACT(r.value(), 1.0f);   // exactly, not 0.99999994
        OMNI_CHECK(r.is_static());
        // A zero-length ramp jumps.
        r.set_target(0.25f, 0);
        OMNI_CHECK_EXACT(r.value(), 0.25f);
        OMNI_CHECK(r.is_static());
    }

    // --- delay line: bypass at zero is exact ----------------------------
    {
        DelayLine d;
        std::vector<float> buf = prng_noise(128, 7u);
        const std::vector<float> before = buf;
        d.process(buf.data(), buf.size());    // no reserve, no delay
        for (std::size_t i = 0; i < buf.size(); ++i) OMNI_CHECK_EXACT(buf[i], before[i]);
    }
    // --- and delays by exactly N frames ---------------------------------
    {
        DelayLine d;
        d.reserve(8);
        d.set_delay(8);
        OMNI_CHECK(d.delay() == 8);
        std::vector<float> buf(32, 0.0f);
        buf[0] = 1.0f;
        d.process(buf.data(), buf.size());
        OMNI_CHECK_EXACT(buf[8], 1.0f);
        for (std::size_t i = 0; i < 32; ++i)
            if (i != 8) OMNI_CHECK_EXACT(buf[i], 0.0f);
        // Clamped to what was reserved rather than reading out of bounds.
        d.set_delay(1000);
        OMNI_CHECK(d.delay() == 8);
    }

    // --- WAV round trip, every format -----------------------------------
    {
        const char* dir = std::getenv("OMNI_TEST_TMPDIR");
        const std::string base = dir ? std::string(dir) : std::string(".");
        io::AudioFile f;
        f.sample_rate = 48000;
        f.channels = 2;
        f.interleaved = prng_noise(2048, 0x99u);

        // Float32 is exact.
        {
            std::string err;
            const std::string path = base + "/omni_wav_f32.wav";
            OMNI_CHECK_MSG(io::write_wav(path, f, io::SampleFormat::Float32, err), err);
            io::AudioFile back;
            OMNI_CHECK_MSG(io::read_wav(path, back, err), err);
            OMNI_CHECK(back.sample_rate == 48000);
            OMNI_CHECK(back.channels == 2);
            OMNI_CHECK(back.interleaved.size() == f.interleaved.size());
            std::size_t bad = 0;
            for (std::size_t i = 0; i < f.interleaved.size() && i < back.interleaved.size(); ++i)
                if (back.interleaved[i] != f.interleaved[i]) ++bad;
            OMNI_CHECK_MSG(bad == 0, "float32 WAV round trip must be exact");
        }
        // Integer formats round-trip to their own quantisation.
        for (auto fmt : {io::SampleFormat::Pcm16, io::SampleFormat::Pcm24,
                         io::SampleFormat::Pcm32}) {
            std::string err;
            const std::string path =
                base + "/omni_wav_" + io::format_name(fmt) + ".wav";
            OMNI_CHECK_MSG(io::write_wav(path, f, fmt, err), err);
            io::AudioFile back;
            OMNI_CHECK_MSG(io::read_wav(path, back, err), err);
            OMNI_CHECK(back.interleaved.size() == f.interleaved.size());
            const double tol = (fmt == io::SampleFormat::Pcm16) ? 1.0 / 32768.0
                                                                : 1.0 / 8388608.0;
            double worst = 0.0;
            for (std::size_t i = 0; i < f.interleaved.size() && i < back.interleaved.size(); ++i)
                worst = std::max(worst, std::fabs(static_cast<double>(back.interleaved[i]) -
                                                  static_cast<double>(f.interleaved[i])));
            char msg[160];
            std::snprintf(msg, sizeof msg, "%s round trip worst error %.3g > %.3g",
                          io::format_name(fmt), worst, tol);
            OMNI_CHECK_MSG(worst <= tol, msg);
        }
    }

    return finish();
}
