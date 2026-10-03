#include "omni/engine/strip.hpp"

#include <algorithm>

namespace omni {
namespace {

// A ramp is shared by both legs of a stereo path, so both legs must be walked in
// the same loop. Applying a ramp per channel would advance it twice per frame
// and halve every smoothing time -- a bug worth naming, because the obvious
// "loop over channels calling apply_gain" has exactly that shape.
void apply_gain_stereo(float* l, float* r, std::size_t n, dsp::Ramp& ramp) noexcept {
    if (ramp.is_static()) {
        const float g = ramp.value();
        if (g == 1.0f) return;  // exact pass-through: the unity path
        if (g == 0.0f) {
            std::fill(l, l + n, 0.0f);
            std::fill(r, r + n, 0.0f);
            return;
        }
        for (std::size_t i = 0; i < n; ++i) {
            l[i] *= g;
            r[i] *= g;
        }
        return;
    }
    for (std::size_t i = 0; i < n; ++i) {
        const float g = ramp.next();
        l[i] *= g;
        r[i] *= g;
    }
}

void apply_pan_stereo(float* l, float* r, std::size_t n,
                      dsp::Ramp& gl, dsp::Ramp& gr) noexcept {
    if (gl.is_static() && gr.is_static()) {
        const float a = gl.value();
        const float b = gr.value();
        if (a == 1.0f && b == 1.0f) return;  // centred balance: exact
        for (std::size_t i = 0; i < n; ++i) {
            l[i] *= a;
            r[i] *= b;
        }
        return;
    }
    for (std::size_t i = 0; i < n; ++i) {
        l[i] *= gl.next();
        r[i] *= gr.next();
    }
}

void mix_stereo(const float* sl, const float* sr, float* dl, float* dr,
                std::size_t n, dsp::Ramp& ramp) noexcept {
    if (ramp.is_static()) {
        const float g = ramp.value();
        if (g == 0.0f) return;
        if (g == 1.0f) {
            // 0.0f + x == x exactly, so a single send at unity onto a zeroed bus
            // reproduces the source bit for bit.
            for (std::size_t i = 0; i < n; ++i) {
                dl[i] += sl[i];
                dr[i] += sr[i];
            }
            return;
        }
        for (std::size_t i = 0; i < n; ++i) {
            dl[i] += sl[i] * g;
            dr[i] += sr[i] * g;
        }
        return;
    }
    for (std::size_t i = 0; i < n; ++i) {
        const float g = ramp.next();
        dl[i] += sl[i] * g;
        dr[i] += sr[i] * g;
    }
}

}  // namespace

void Strip::process(float* const work[kChannels], std::size_t frames,
                    float* const* bus_bufs, std::size_t num_buses) noexcept {
    float* l = work[0];
    float* r = work[1];

    // A mono strip carries its source on the left leg; the pan law places it.
    if (mode_ == ChannelMode::Mono) {
        std::copy(l, l + frames, r);
    }

    // 1. Polarity (7.1). Negation is exact, and -(-x) == x.
    if (polarity_invert_) {
        for (std::size_t i = 0; i < frames; ++i) {
            l[i] = -l[i];
            r[i] = -r[i];
        }
    }

    // 2. Trim / Gain knob.
    apply_gain_stereo(l, r, frames, trim_);

    // 3. Input delay (lip-sync). Bypassed at zero.
    delay_[0].process(l, frames);
    delay_[1].process(r, frames);

    // Tap 1 INPUT: input meter and clip LED.
    input_meter_.process(l, frames);
    input_meter_.process(r, frames);

    // ( Phase 2 inserts -- HPF, gate, EQ, compressor -- belong here, 7.1. )

    // 4. Fader.
    apply_gain_stereo(l, r, frames, fader_);

    // 5. Pan or balance.
    apply_pan_stereo(l, r, frames, pan_l_, pan_r_);

    // 6. Mute, ramped, applied to every send (7.2).
    apply_gain_stereo(l, r, frames, mute_);

    // Tap 4 POST-FADER: the strip meter the Simple view shows.
    post_meter_.process(l, frames);
    post_meter_.process(r, frames);

    // 7. Send matrix -- the A/B buttons.
    const std::size_t n = std::min(num_buses, sends_.size());
    for (std::size_t b = 0; b < n; ++b) {
        Send& s = sends_[b];
        if (s.gain.is_static() && s.gain.value() == 0.0f) continue;  // not routed
        mix_stereo(l, r, bus_bufs[b * kChannels], bus_bufs[b * kChannels + 1],
                   frames, s.gain);
    }
}

}  // namespace omni
