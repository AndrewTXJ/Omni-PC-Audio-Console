// Parameter smoothing (roadmap 7.2: level changes 10-20 ms, mutes 5-10 ms).
//
// Two properties the rest of the engine depends on:
//
//   1. A settled ramp holds its target EXACTLY. It does not approach it
//      asymptotically, because QS-01 needs a fader at 0 dB to be 1.0f and not
//      1.0f-epsilon. That rules out the usual one-pole smoother.
//
//   2. is_static() lets callers skip the per-sample path entirely. A static
//      unity gain becomes a no-op, which is both the fast path and the
//      bit-exact path.
#pragma once

#include <cstddef>

namespace omni::dsp {

class Ramp {
  public:
    Ramp() = default;
    explicit Ramp(float initial) noexcept : current_(initial), target_(initial) {}

    /// Jump with no ramp. For initialisation and recall, never mid-stream.
    void reset(float value) noexcept {
        current_ = target_ = value;
        remaining_ = 0;
    }

    /// Ramp to `value` over `frames`. Zero frames jumps immediately.
    void set_target(float value, std::size_t frames) noexcept {
        if (frames == 0 || value == current_) {
            reset(value);
            return;
        }
        target_ = value;
        remaining_ = frames;
        step_ = (target_ - current_) / static_cast<float>(frames);
    }

    /// True when the value is constant, so a block can use one scalar multiply.
    [[nodiscard]] bool is_static() const noexcept { return remaining_ == 0; }

    /// The current value. Meaningful only while is_static().
    [[nodiscard]] float value() const noexcept { return current_; }
    [[nodiscard]] float target() const noexcept { return target_; }

    /// Advance one frame and return the new value.
    float next() noexcept {
        if (remaining_ == 0) return current_;
        if (--remaining_ == 0) {
            current_ = target_;  // land exactly, never near
        } else {
            current_ += step_;
        }
        return current_;
    }

    /// Advance `frames` without producing values (for bypassed paths).
    void skip(std::size_t frames) noexcept {
        for (std::size_t i = 0; i < frames && remaining_ > 0; ++i) next();
    }

  private:
    float current_ = 0.0f;
    float target_ = 0.0f;
    float step_ = 0.0f;
    std::size_t remaining_ = 0;
};

/// Multiply a block by a ramp. No-op when the ramp is a settled unity gain,
/// which is what keeps the unity path bit-exact.
inline void apply_gain(float* data, std::size_t n, Ramp& ramp) noexcept {
    if (ramp.is_static()) {
        const float g = ramp.value();
        if (g == 1.0f) return;              // exact pass-through
        if (g == 0.0f) {
            for (std::size_t i = 0; i < n; ++i) data[i] = 0.0f;
            return;
        }
        for (std::size_t i = 0; i < n; ++i) data[i] *= g;
        return;
    }
    for (std::size_t i = 0; i < n; ++i) data[i] *= ramp.next();
}

/// Accumulate src * ramp into dst.
inline void mix_gain(const float* src, float* dst, std::size_t n, Ramp& ramp) noexcept {
    if (ramp.is_static()) {
        const float g = ramp.value();
        if (g == 0.0f) return;
        if (g == 1.0f) {
            for (std::size_t i = 0; i < n; ++i) dst[i] += src[i];
            return;
        }
        for (std::size_t i = 0; i < n; ++i) dst[i] += src[i] * g;
        return;
    }
    for (std::size_t i = 0; i < n; ++i) dst[i] += src[i] * ramp.next();
}

}  // namespace omni::dsp
