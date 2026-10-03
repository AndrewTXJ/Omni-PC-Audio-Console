// Safety limiter -- last in every bus, always (roadmap 4.3 [P0], 7.1).
//
// Phase 1 scope, stated plainly: this is a ZERO-LATENCY ceiling. Below the
// ceiling it is the identity, so it does not disturb QS-01's bit-exact unity
// path; above it, it clamps.
//
// What it is NOT: it does not control inter-sample (true) peaks, because doing
// that needs look-ahead, and look-ahead is delay, and delay would break the
// unity null test. 7.5's look-ahead true-peak limiter replaces this in Phase 2
// and is what QS-14 ultimately measures. Until then, a reconstructed analogue
// peak can exceed the ceiling even though no sample does.
//
// It cannot be bypassed. That is the point of it: 4.3 makes it P0 and principle
// 4 is "never leave the PC silent or noisy".
#pragma once

#include <cstddef>

#include "omni/dsp/guards.hpp"

namespace omni::dsp {

class SafetyLimiter {
  public:
    /// Ceiling as a linear amplitude. Defaults to full scale.
    void set_ceiling(float ceiling) noexcept {
        ceiling_ = (ceiling > 0.0f) ? ceiling : 0.0f;
    }
    [[nodiscard]] float ceiling() const noexcept { return ceiling_; }

    /// True once any sample has been clamped since the last clear.
    [[nodiscard]] bool engaged() const noexcept { return engaged_; }
    void clear_engaged() noexcept { engaged_ = false; }

    void process(float* data, std::size_t n) noexcept {
        const float c = ceiling_;
        for (std::size_t i = 0; i < n; ++i) {
            float x = sanitize(data[i]);   // identity for valid audio
            if (x > c) {
                x = c;
                engaged_ = true;
            } else if (x < -c) {
                x = -c;
                engaged_ = true;
            }
            data[i] = x;
        }
    }

  private:
    float ceiling_ = 1.0f;
    bool engaged_ = false;
};

}  // namespace omni::dsp
