// Deterministic test signals. No Date/random seeding: every vector is
// reproducible so a CI failure is reproducible.
#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "omni/dsp/db.hpp"
#include "omni/dsp/guards.hpp"

namespace omni::test {

inline constexpr double kPi = 3.14159265358979323846;

/// Sine at a given level in dBFS.
inline std::vector<float> sine(std::size_t frames, double hz, double rate,
                               float level_db, double phase = 0.0) {
    std::vector<float> out(frames);
    const double amp = static_cast<double>(dsp::db_to_gain(level_db));
    for (std::size_t i = 0; i < frames; ++i) {
        out[i] = static_cast<float>(amp * std::sin(2.0 * kPi * hz *
                                                   static_cast<double>(i) / rate + phase));
    }
    return out;
}

/// xorshift32 noise in [-1, 1). Fixed seed: same vector on every machine.
inline std::vector<float> prng_noise(std::size_t frames, std::uint32_t seed = 0x12345678u) {
    std::vector<float> out(frames);
    std::uint32_t s = seed;
    for (std::size_t i = 0; i < frames; ++i) {
        s ^= s << 13;
        s ^= s >> 17;
        s ^= s << 5;
        out[i] = static_cast<float>(static_cast<std::int32_t>(s)) * (1.0f / 2147483648.0f);
    }
    return out;
}

/// Values exactly representable at the given bit depth, so a null test proves
/// transparency for real 16- and 24-bit material rather than for floats that
/// happen to survive.
inline std::vector<float> quantized_noise(std::size_t frames, int bits,
                                          std::uint32_t seed = 0xC0FFEEu) {
    std::vector<float> out = prng_noise(frames, seed);
    for (auto& v : out) {
        if (bits == 16) v = dsp::from_i16(dsp::to_i16(v));
        else if (bits == 24) v = dsp::from_i24(dsp::to_i24(v));
    }
    return out;
}

inline std::vector<float> impulse(std::size_t frames, std::size_t at, float amp) {
    std::vector<float> out(frames, 0.0f);
    if (at < frames) out[at] = amp;
    return out;
}

inline float peak(const float* d, std::size_t n) {
    float p = 0.0f;
    for (std::size_t i = 0; i < n; ++i) p = std::max(p, std::fabs(d[i]));
    return p;
}

inline double rms(const float* d, std::size_t n) {
    double acc = 0.0;
    for (std::size_t i = 0; i < n; ++i)
        acc += static_cast<double>(d[i]) * static_cast<double>(d[i]);
    return std::sqrt(acc / static_cast<double>(n));
}

}  // namespace omni::test
