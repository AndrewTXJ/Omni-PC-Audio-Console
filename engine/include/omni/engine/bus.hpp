// Output bus (roadmap 4.3 [P0, S] and the bus flow in 7.1).
//
//   sum of sends -> [inserts: Phase 2] -> fader -> mute -> output delay
//                -> safety limiter (always last) -> output patch
//
// The sum is accumulated onto a buffer zeroed at the top of every block, which
// is what makes an unpatched bus bit-exact silence (QS-04) rather than merely
// quiet.
#pragma once

#include <cstddef>

#include "omni/dsp/db.hpp"
#include "omni/dsp/limiter.hpp"
#include "omni/dsp/meter.hpp"
#include "omni/dsp/ramp.hpp"
#include "omni/engine/delay.hpp"
#include "omni/engine/types.hpp"

namespace omni {

class Bus {
  public:
    void configure(double sample_rate) {
        sample_rate_ = sample_rate;
        level_frames_ = ms_to_frames(kLevelSmoothingMs, sample_rate);
        mute_frames_ = ms_to_frames(kMuteSmoothingMs, sample_rate);
        fader_.reset(1.0f);
        mute_.reset(1.0f);
    }

    void set_fader_db(float db) noexcept {
        fader_db_ = (db < dsp::kSilenceDb) ? dsp::kSilenceDb : db;
        if (fader_db_ > kFaderMaxDb) fader_db_ = kFaderMaxDb;
        fader_.set_target(dsp::db_to_gain(fader_db_), level_frames_);
    }
    [[nodiscard]] float fader_db() const noexcept { return fader_db_; }

    void set_mute(bool muted) noexcept {
        muted_ = muted;
        mute_.set_target(muted ? 0.0f : 1.0f, mute_frames_);
    }
    [[nodiscard]] bool muted() const noexcept { return muted_; }

    /// MONO button (4.3): sum both legs. Applied after the fader.
    void set_mono(bool mono) noexcept { mono_ = mono; }
    [[nodiscard]] bool mono() const noexcept { return mono_; }

    void reserve_output_delay(std::size_t frames) {
        for (auto& d : delay_) d.reserve(frames);
    }
    void set_output_delay_frames(std::size_t frames) noexcept {
        for (auto& d : delay_) d.set_delay(frames);
    }

    /// Jump every ramp to its target. Initial state only -- see Strip::snap().
    void snap() noexcept {
        fader_.reset(fader_.target());
        mute_.reset(mute_.target());
    }

    [[nodiscard]] dsp::SafetyLimiter& limiter() noexcept { return limiter_; }
    [[nodiscard]] dsp::PeakMeter& meter() noexcept { return meter_; }

    /// Audio thread. Operates in place on the bus accumulation buffers.
    void process(float* const bufs[kChannels], std::size_t frames) noexcept;

  private:
    double sample_rate_ = 48000.0;
    std::size_t level_frames_ = 0;
    std::size_t mute_frames_ = 0;

    bool muted_ = false;
    bool mono_ = false;
    float fader_db_ = 0.0f;

    dsp::Ramp fader_{1.0f};
    dsp::Ramp mute_{1.0f};
    DelayLine delay_[kChannels];
    dsp::SafetyLimiter limiter_;
    dsp::PeakMeter meter_;
};

}  // namespace omni
