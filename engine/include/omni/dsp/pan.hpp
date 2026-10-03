// Pan and balance laws (roadmap 7.2: "Pan law selectable: 0, -3 (default),
// -4.5 or -6 dB"; "Pan on mono strips, balance on stereo strips").
//
// The laws are defined here, in formulas, because QS-05 checks the engine
// against "the law's formula" and a law nobody wrote down cannot be checked.
//
// PAN (mono source placed in a stereo field), for laws below 0 dB:
//
//     theta = (pos + 1) * pi/4          pos in [-1, +1]
//     left  = cos(theta)^p
//     right = sin(theta)^p
//     p     = ln(c) / ln(cos(pi/4))     c = the law's centre gain
//
// One family covers all three: p is chosen so the centre lands exactly on the
// law's nominal attenuation. p == 1 is the familiar constant-power law.
//
// The 0 dB law is NOT in that family -- p would be 0 and nothing would pan. It
// is instead the "no attenuation at centre" curve, identical to the balance
// curve below: full gain on the near leg, the far leg taken down linearly.
//
// BALANCE (stereo source), used on stereo strips:
//
//     left  = pos <= 0 ? 1 : 1 - pos
//     right = pos >= 0 ? 1 : 1 + pos
//
// Centred balance is therefore EXACTLY 1.0 on both legs, which is what QS-01's
// bit-exact unity path requires -- it specifies "a stereo strip with balance
// centred" for precisely this reason.
#pragma once

#include <cmath>

#include "omni/dsp/db.hpp"

namespace omni::dsp {

enum class PanLaw { ZeroDb, Minus3Db, Minus4_5Db, Minus6Db };

struct StereoGain {
    float left;
    float right;
};

/// The law's nominal centre attenuation in dB.
[[nodiscard]] inline float pan_law_centre_db(PanLaw law) noexcept {
    switch (law) {
        case PanLaw::ZeroDb:     return 0.0f;
        case PanLaw::Minus3Db:   return -3.0f;
        case PanLaw::Minus4_5Db: return -4.5f;
        case PanLaw::Minus6Db:   return -6.0f;
    }
    return -3.0f;
}

/// Pan a mono source. `pos` is -1 (hard left) to +1 (hard right).
[[nodiscard]] inline StereoGain pan_gains(float pos, PanLaw law) noexcept {
    if (pos < -1.0f) pos = -1.0f;
    if (pos > 1.0f) pos = 1.0f;

    if (law == PanLaw::ZeroDb) {
        return {pos <= 0.0f ? 1.0f : 1.0f - pos,
                pos >= 0.0f ? 1.0f : 1.0f + pos};
    }

    const float c = db_to_gain(pan_law_centre_db(law));
    // ln(c) / ln(cos(pi/4)); cos(pi/4) = 1/sqrt(2).
    const float p = std::log(c) / std::log(0.70710678118654752440f);
    const float theta = (pos + 1.0f) * 0.78539816339744830961f;  // * pi/4
    const float cs = std::cos(theta);
    const float sn = std::sin(theta);
    return {cs <= 0.0f ? 0.0f : std::pow(cs, p),
            sn <= 0.0f ? 0.0f : std::pow(sn, p)};
}

/// Balance a stereo source. Centred is exactly unity on both legs.
[[nodiscard]] inline StereoGain balance_gains(float pos) noexcept {
    if (pos < -1.0f) pos = -1.0f;
    if (pos > 1.0f) pos = 1.0f;
    return {pos <= 0.0f ? 1.0f : 1.0f - pos,
            pos >= 0.0f ? 1.0f : 1.0f + pos};
}

}  // namespace omni::dsp
