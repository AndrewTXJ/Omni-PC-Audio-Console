// Decibel <-> linear gain conversion.
//
// The one hard requirement: 0 dB must map to EXACTLY 1.0f. QS-01 asks the unity
// path to be bit-exact, and multiplying a float by exactly 1.0f is bit-exact for
// every finite value under IEEE 754, so the whole null test rests on this
// function not returning 0.99999994f.
#pragma once

#include <cmath>
#include <limits>

namespace omni::dsp {

/// Faders reaching this value or below are silent (-inf on the scale, 7.2).
inline constexpr float kSilenceDb = -144.0f;

/// dB to linear amplitude gain. Exactly 1.0f at 0 dB, exactly 0.0f at silence.
[[nodiscard]] inline float db_to_gain(float db) noexcept {
    if (db == 0.0f) return 1.0f;                 // exact, and the common case
    if (db <= kSilenceDb) return 0.0f;           // -inf end of the fader
    return std::pow(10.0f, db * 0.05f);          // 10^(dB/20)
}

/// Linear amplitude gain to dB. Returns -inf for zero or negative gain.
[[nodiscard]] inline float gain_to_db(float gain) noexcept {
    if (gain == 1.0f) return 0.0f;               // exact round-trip at unity
    if (gain <= 0.0f) return -std::numeric_limits<float>::infinity();
    return 20.0f * std::log10(gain);
}

}  // namespace omni::dsp
