// Peak metering with a latched clip indicator (roadmap 4.3, 7.4).
//
// 7.4 requires meters to be computed in the engine and published through
// lock-free shared memory, "so a busy UI can never disturb the audio". The
// engine side writes with relaxed atomics and never reads back; the UI side
// reads and clears. No lock, no allocation, no syscall on the audio thread.
//
// This is the Phase 1 digital peak meter. True-peak (4x oversampled, BS.1770),
// RMS/VU and LUFS are Phase 2 (7.4) and are deliberately absent.
#pragma once

#include <atomic>
#include <cmath>
#include <cstddef>

namespace omni::dsp {

/// Full scale. A sample strictly above this latches the clip indicator.
inline constexpr float kClipThreshold = 1.0f;

class PeakMeter {
  public:
    PeakMeter() = default;

    // std::atomic is neither copyable nor movable, which would make every Strip
    // and Bus immovable and so unable to live in a contiguous vector. These
    // snapshot the published values instead. They exist for configure() on the
    // CONTROL thread, when no audio is running; moving a meter while the audio
    // thread is touching it is not something they make safe.
    PeakMeter(const PeakMeter& other) noexcept {
        peak_.store(other.peak_.load(std::memory_order_relaxed),
                    std::memory_order_relaxed);
        clipped_.store(other.clipped_.load(std::memory_order_relaxed),
                       std::memory_order_relaxed);
    }
    PeakMeter& operator=(const PeakMeter& other) noexcept {
        if (this != &other) {
            peak_.store(other.peak_.load(std::memory_order_relaxed),
                        std::memory_order_relaxed);
            clipped_.store(other.clipped_.load(std::memory_order_relaxed),
                           std::memory_order_relaxed);
        }
        return *this;
    }
    PeakMeter(PeakMeter&& other) noexcept
        : PeakMeter(static_cast<const PeakMeter&>(other)) {}
    PeakMeter& operator=(PeakMeter&& other) noexcept {
        return *this = static_cast<const PeakMeter&>(other);
    }
    ~PeakMeter() = default;

    /// Audio thread: fold a block into the published peak.
    void process(const float* data, std::size_t n) noexcept {
        float peak = 0.0f;
        bool clipped = false;
        for (std::size_t i = 0; i < n; ++i) {
            const float a = std::fabs(data[i]);
            if (a > peak) peak = a;
            if (a > kClipThreshold) clipped = true;
        }
        // Hold the maximum since the last UI read.
        float prev = peak_.load(std::memory_order_relaxed);
        if (peak > prev) peak_.store(peak, std::memory_order_relaxed);
        if (clipped) clipped_.store(true, std::memory_order_relaxed);
    }

    /// UI thread: read and reset the peak hold. The clip latch persists until
    /// clear_clip(), because a clip the user never saw may as well not have been
    /// indicated (4.3 calls for a latched clip LED).
    [[nodiscard]] float read_peak() noexcept {
        return peak_.exchange(0.0f, std::memory_order_relaxed);
    }
    [[nodiscard]] float peek_peak() const noexcept {
        return peak_.load(std::memory_order_relaxed);
    }
    [[nodiscard]] bool clipped() const noexcept {
        return clipped_.load(std::memory_order_relaxed);
    }
    void clear_clip() noexcept { clipped_.store(false, std::memory_order_relaxed); }

    void reset() noexcept {
        peak_.store(0.0f, std::memory_order_relaxed);
        clipped_.store(false, std::memory_order_relaxed);
    }

  private:
    std::atomic<float> peak_{0.0f};
    std::atomic<bool> clipped_{false};
};

static_assert(std::atomic<float>::is_always_lock_free,
              "the meter bridge must be lock-free on the audio thread");

}  // namespace omni::dsp
