// Channel strip (roadmap 4.3 [P0, S] and the flow in 7.1).
//
// Order, matching 7.1 exactly:
//
//   source -> polarity -> trim -> input delay -> [HPF/inserts: Phase 2]
//          -> fader -> pan or balance -> mute -> send matrix -> buses
//
// Tap points: 1 INPUT (input meter), 4 POST-FADER (strip meter). Taps 2
// (pre-insert) and 3 (pre-fader) arrive with the inserts they exist to feed.
//
// THE UNITY PATH. With polarity off, trim 0 dB, no delay, fader 0 dB, balance
// centred, mute off and a send at 0 dB, every multiplier in this chain is
// exactly 1.0f and the accumulate is onto a zeroed bus. Output equals input,
// sample for sample -- QS-01.
#pragma once

#include <cstddef>
#include <vector>

#include "omni/dsp/db.hpp"
#include "omni/dsp/meter.hpp"
#include "omni/dsp/pan.hpp"
#include "omni/dsp/ramp.hpp"
#include "omni/engine/delay.hpp"
#include "omni/engine/types.hpp"

namespace omni {

struct Send {
    dsp::Ramp gain{0.0f};
    float level_db = 0.0f;   // an A/B button is a send at 0 dB (roadmap 3)
    bool enabled = false;
};

class Strip {
  public:
    /// Control thread. Allocates; never call from the audio thread.
    void configure(double sample_rate, std::size_t num_buses, ChannelMode mode) {
        sample_rate_ = sample_rate;
        mode_ = mode;
        level_frames_ = ms_to_frames(kLevelSmoothingMs, sample_rate);
        mute_frames_ = ms_to_frames(kMuteSmoothingMs, sample_rate);
        sends_.assign(num_buses, Send{});
        trim_.reset(1.0f);
        fader_.reset(1.0f);
        mute_.reset(1.0f);
        apply_pan_position();
    }

    // ---- parameters (control thread, between blocks) ---------------------
    void set_polarity_invert(bool invert) noexcept { polarity_invert_ = invert; }
    [[nodiscard]] bool polarity_invert() const noexcept { return polarity_invert_; }

    void set_trim_db(float db) noexcept {
        trim_db_ = clamp(db, kTrimMinDb, kTrimMaxDb);
        trim_.set_target(dsp::db_to_gain(trim_db_), level_frames_);
    }
    [[nodiscard]] float trim_db() const noexcept { return trim_db_; }

    void set_fader_db(float db) noexcept {
        fader_db_ = (db < dsp::kSilenceDb) ? dsp::kSilenceDb : clamp(db, dsp::kSilenceDb, kFaderMaxDb);
        fader_.set_target(dsp::db_to_gain(fader_db_), level_frames_);
    }
    [[nodiscard]] float fader_db() const noexcept { return fader_db_; }

    void set_mute(bool muted) noexcept {
        muted_ = muted;
        mute_.set_target(muted ? 0.0f : 1.0f, mute_frames_);
    }
    [[nodiscard]] bool muted() const noexcept { return muted_; }

    /// -1 hard left to +1 hard right. Pan on mono strips, balance on stereo (7.2).
    void set_pan(float pos) noexcept {
        pan_pos_ = clamp(pos, -1.0f, 1.0f);
        apply_pan_position();
    }
    [[nodiscard]] float pan() const noexcept { return pan_pos_; }

    void set_pan_law(dsp::PanLaw law) noexcept {
        pan_law_ = law;
        apply_pan_position();
    }
    [[nodiscard]] dsp::PanLaw pan_law() const noexcept { return pan_law_; }

    /// An A/B button: enable or disable the send to `bus`.
    void set_send_enabled(std::size_t bus, bool enabled) noexcept {
        if (bus >= sends_.size()) return;
        sends_[bus].enabled = enabled;
        update_send(bus);
    }
    /// Expert panel: the send's level (7.2).
    void set_send_level_db(std::size_t bus, float db) noexcept {
        if (bus >= sends_.size()) return;
        sends_[bus].level_db = db;
        update_send(bus);
    }
    [[nodiscard]] bool send_enabled(std::size_t bus) const noexcept {
        return bus < sends_.size() && sends_[bus].enabled;
    }

    /// Whether the send currently carries audio, which is not the same as being
    /// enabled: a send switched off is still fading for a few milliseconds. The
    /// scheduler and the loop detector both need the conservative answer, because
    /// a dependency that exists for 7 ms is still a dependency.
    [[nodiscard]] bool send_active(std::size_t bus) const noexcept {
        if (bus >= sends_.size()) return false;
        const Send& s = sends_[bus];
        if (s.enabled) return true;
        return !(s.gain.is_static() && s.gain.value() == 0.0f);
    }
    [[nodiscard]] std::size_t num_sends() const noexcept { return sends_.size(); }

    /// Control thread: reserve and set the input delay (7.2, lip-sync).
    void reserve_input_delay(std::size_t frames) {
        for (auto& d : delay_) d.reserve(frames);
    }
    void set_input_delay_frames(std::size_t frames) noexcept {
        for (auto& d : delay_) d.set_delay(frames);
    }

    /// Jump every ramp to its target, with no fade.
    ///
    /// For INITIAL state only: loading a session, or an offline render, where
    /// there is no device to protect and a fade-in would corrupt the first few
    /// milliseconds of the result. A live profile recall must NOT use this --
    /// 4.9 requires those to ramp, and 4.1 restores after a crash "with a short
    /// fade-in". The offline renderer calls it because a render that fades in
    /// from silence is not the steady-state answer anyone asked for.
    void snap() noexcept {
        trim_.reset(trim_.target());
        fader_.reset(fader_.target());
        mute_.reset(mute_.target());
        pan_l_.reset(pan_l_.target());
        pan_r_.reset(pan_r_.target());
        for (Send& s : sends_) s.gain.reset(s.gain.target());
    }

    [[nodiscard]] dsp::PeakMeter& input_meter() noexcept { return input_meter_; }
    [[nodiscard]] dsp::PeakMeter& post_fader_meter() noexcept { return post_meter_; }

    /// Audio thread. `work` is the strip's stereo scratch, pre-loaded with the
    /// source; `buses` is the bus accumulation area. Allocates nothing.
    void process(float* const work[kChannels], std::size_t frames,
                 float* const* bus_bufs, std::size_t num_buses) noexcept;

  private:
    static float clamp(float v, float lo, float hi) noexcept {
        return v < lo ? lo : (v > hi ? hi : v);
    }

    void apply_pan_position() noexcept {
        const dsp::StereoGain g = (mode_ == ChannelMode::Stereo)
                                      ? dsp::balance_gains(pan_pos_)
                                      : dsp::pan_gains(pan_pos_, pan_law_);
        pan_l_.set_target(g.left, level_frames_);
        pan_r_.set_target(g.right, level_frames_);
    }

    void update_send(std::size_t bus) noexcept {
        Send& s = sends_[bus];
        const float target = s.enabled ? dsp::db_to_gain(s.level_db) : 0.0f;
        s.gain.set_target(target, mute_frames_);
    }

    double sample_rate_ = 48000.0;
    ChannelMode mode_ = ChannelMode::Stereo;
    std::size_t level_frames_ = 0;
    std::size_t mute_frames_ = 0;

    bool polarity_invert_ = false;
    bool muted_ = false;
    float trim_db_ = 0.0f;
    float fader_db_ = 0.0f;
    float pan_pos_ = 0.0f;
    dsp::PanLaw pan_law_ = dsp::PanLaw::Minus3Db;

    dsp::Ramp trim_{1.0f};
    dsp::Ramp fader_{1.0f};
    dsp::Ramp mute_{1.0f};
    dsp::Ramp pan_l_{1.0f};
    dsp::Ramp pan_r_{1.0f};

    DelayLine delay_[kChannels];
    dsp::PeakMeter input_meter_;
    dsp::PeakMeter post_meter_;
    std::vector<Send> sends_;
};

}  // namespace omni
