// Numeric guards (roadmap 4.1).
//
// WHY THESE ARE NOT ALL APPLIED EVERYWHERE
//
// 4.1 asks for "NaN/Inf and denormal protection on every processor output".
// 7.7's first row asks the untouched path to be bit-exact. Those two cannot both
// hold literally: flushing a denormal INPUT sample changes it, so a null test
// fed a denormal would fail.
//
// The resolution, which the test plan records as a deliberate decision:
//
//   * sanitize() -- replacing non-finite values with silence -- is applied at
//     every bus output. For valid audio it is the identity, so it costs nothing
//     in exactness, and it is what stops one NaN from a plugin (5.2) poisoning
//     an entire mix.
//
//   * kill_denormal() is NOT applied to the signal path. Denormals matter where
//     they are GENERATED AND PERSIST -- recursive filter state, reverb tails --
//     because that is where they stall the CPU. A denormal passing through a
//     gain stage is -700 dBFS and harmless. So this is a tool for IIR state
//     (Phase 2 EQ, 7.5), not a pass-through filter.
//
// Flushing denormals globally via MXCSR (FTZ/DAZ) is therefore also rejected:
// it would silently break QS-01 for the whole process.
#pragma once

#include <cmath>
#include <cstdint>
#include <limits>

namespace omni::dsp {

/// Smallest normal float. Below this, recursive state should be flushed.
inline constexpr float kDenormalFloor = 1e-30f;

/// Replace NaN and +/-Inf with silence. Identity for every finite value, so it
/// preserves bit-exactness. Written as a select rather than a branch so the
/// compiler can vectorise it.
[[nodiscard]] inline float sanitize(float x) noexcept {
    return std::isfinite(x) ? x : 0.0f;
}

/// Flush a denormal to zero. For recursive state only -- see the file comment.
[[nodiscard]] inline float kill_denormal(float x) noexcept {
    return (std::fabs(x) < kDenormalFloor) ? 0.0f : x;
}

inline void sanitize_block(float* data, std::size_t n) noexcept {
    for (std::size_t i = 0; i < n; ++i) data[i] = sanitize(data[i]);
}

/// Float to 16-bit integer, saturating. 4.1 requires saturation rather than
/// wrapping: a wrap turns a mild overshoot into full-scale noise, which is a
/// hearing-safety matter, not merely a correctness one.
[[nodiscard]] inline std::int16_t to_i16(float x) noexcept {
    if (!std::isfinite(x)) return 0;
    const float scaled = x * 32768.0f;
    if (scaled >= 32767.0f) return 32767;
    if (scaled <= -32768.0f) return -32768;
    return static_cast<std::int16_t>(std::lrintf(scaled));
}

/// Float to 24-bit integer (in the low 24 bits of an int32), saturating.
[[nodiscard]] inline std::int32_t to_i24(float x) noexcept {
    if (!std::isfinite(x)) return 0;
    const float scaled = x * 8388608.0f;
    if (scaled >= 8388607.0f) return 8388607;
    if (scaled <= -8388608.0f) return -8388608;
    return static_cast<std::int32_t>(std::lrintf(scaled));
}

/// Float to 32-bit integer, saturating.
[[nodiscard]] inline std::int32_t to_i32(float x) noexcept {
    if (!std::isfinite(x)) return 0;
    const double scaled = static_cast<double>(x) * 2147483648.0;
    if (scaled >= 2147483647.0) return 2147483647;
    if (scaled <= -2147483648.0) return -2147483648;
    return static_cast<std::int32_t>(std::llrint(scaled));
}

[[nodiscard]] inline float from_i16(std::int16_t v) noexcept {
    return static_cast<float>(v) * (1.0f / 32768.0f);
}
[[nodiscard]] inline float from_i24(std::int32_t v) noexcept {
    return static_cast<float>(v) * (1.0f / 8388608.0f);
}
[[nodiscard]] inline float from_i32(std::int32_t v) noexcept {
    return static_cast<float>(static_cast<double>(v) * (1.0 / 2147483648.0));
}

}  // namespace omni::dsp
