#include "omni/engine/bus.hpp"

#include <algorithm>

namespace omni {
namespace {

void apply_gain_stereo(float* l, float* r, std::size_t n, dsp::Ramp& ramp) noexcept {
    if (ramp.is_static()) {
        const float g = ramp.value();
        if (g == 1.0f) return;
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

}  // namespace

void Bus::process(float* const bufs[kChannels], std::size_t frames) noexcept {
    float* l = bufs[0];
    float* r = bufs[1];

    // ( Phase 2 bus inserts -- EQ, compressor, look-ahead limiter -- go here. )

    apply_gain_stereo(l, r, frames, fader_);

    // MONO (4.3). Halved so a centred source keeps its level rather than
    // gaining 6 dB; a mono check that changes loudness is not a check.
    if (mono_) {
        for (std::size_t i = 0; i < frames; ++i) {
            const float m = (l[i] + r[i]) * 0.5f;
            l[i] = m;
            r[i] = m;
        }
    }

    apply_gain_stereo(l, r, frames, mute_);

    // Output delay for alignment (7.2). Bypassed at zero.
    delay_[0].process(l, frames);
    delay_[1].process(r, frames);

    // Safety limiter, always last (4.3, 7.1). Also sanitises non-finite values,
    // so one NaN cannot escape a bus into a device.
    limiter_.process(l, frames);
    limiter_.process(r, frames);

    meter_.process(l, frames);
    meter_.process(r, frames);
}

}  // namespace omni
