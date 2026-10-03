// Shared engine types and limits (roadmap 7.2 for the ranges).
#pragma once

#include <cstddef>

namespace omni {

/// Trim / Gain knob range, 7.2: +/-24 dB in 0.1 dB steps.
inline constexpr float kTrimMinDb = -24.0f;
inline constexpr float kTrimMaxDb = 24.0f;

/// Fader range, 7.2: -inf to +12 dB, 0 dB = unity.
inline constexpr float kFaderMaxDb = 12.0f;

/// Smoothing times, 7.2: levels 10-20 ms, mutes 5-10 ms.
inline constexpr float kLevelSmoothingMs = 15.0f;
inline constexpr float kMuteSmoothingMs = 7.5f;

/// Every internal path is stereo in Phase 1. Surround virtual devices (4.2) are
/// downmixed at the device boundary, not carried through the graph.
inline constexpr std::size_t kChannels = 2;

enum class ChannelMode { Mono, Stereo };

[[nodiscard]] inline std::size_t ms_to_frames(float ms, double sample_rate) noexcept {
    if (ms <= 0.0f) return 0;
    return static_cast<std::size_t>((static_cast<double>(ms) / 1000.0) * sample_rate + 0.5);
}

}  // namespace omni
