// Integer-frame delay line (roadmap 7.2: input delay 0-2000 ms for lip-sync,
// output delay per bus for alignment).
//
// Bypassed at zero: no copy, no state, no arithmetic, so a path with no delay
// set stays bit-exact for QS-01. 7.2 asks for the buffer to be "allocated only
// when used, off the audio thread" -- hence reserve(), which the control thread
// calls, and a process() that never allocates.
#pragma once

#include <algorithm>
#include <cstddef>
#include <vector>

namespace omni {

class DelayLine {
  public:
    /// Control thread only: make room for up to `frames` of delay.
    void reserve(std::size_t frames) {
        if (frames == 0) return;
        const std::size_t need = frames + 1;
        if (buffer_.size() < need * 2) {
            buffer_.assign(need * 2, 0.0f);
            write_ = 0;
        }
        capacity_ = frames;
    }

    /// Control thread only. Clamped to whatever reserve() made room for.
    void set_delay(std::size_t frames) noexcept {
        delay_ = std::min(frames, capacity_);
    }

    [[nodiscard]] std::size_t delay() const noexcept { return delay_; }

    void clear() noexcept {
        std::fill(buffer_.begin(), buffer_.end(), 0.0f);
        write_ = 0;
    }

    /// In-place delay of one channel's block. No-op at zero delay.
    void process(float* data, std::size_t n) noexcept {
        if (delay_ == 0 || buffer_.empty()) return;
        const std::size_t ring = buffer_.size();
        for (std::size_t i = 0; i < n; ++i) {
            const std::size_t read = (write_ + ring - delay_) % ring;
            const float out = buffer_[read];
            buffer_[write_] = data[i];
            data[i] = out;
            write_ = (write_ + 1) % ring;
        }
    }

  private:
    std::vector<float> buffer_;
    std::size_t capacity_ = 0;
    std::size_t delay_ = 0;
    std::size_t write_ = 0;
};

}  // namespace omni
