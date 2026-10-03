#include "omni/engine/patch.hpp"

#include <algorithm>

#include "omni/dsp/db.hpp"

namespace omni {

const char* patch_result_name(PatchResult r) noexcept {
    switch (r) {
        case PatchResult::Ok: return "ok";
        case PatchResult::OutOfRange: return "endpoint out of range";
        case PatchResult::WrongDirection:
            return "a patch runs from a bus output to a strip input";
        case PatchResult::Duplicate: return "that patch already exists";
        case PatchResult::WouldLoop: return "that patch would create a feedback loop";
    }
    return "unknown";
}

void PatchBay::configure(std::size_t num_strips, std::size_t num_buses,
                         double sample_rate) {
    num_strips_ = num_strips;
    num_buses_ = num_buses;
    crossfade_frames_ = ms_to_frames(kPatchCrossfadeMs, sample_rate);
    entries_.clear();
}

PatchResult PatchBay::validate(const Endpoint& from, const Endpoint& to) const noexcept {
    if (from.kind != EndpointKind::BusOutput || to.kind != EndpointKind::StripInput)
        return PatchResult::WrongDirection;
    if (from.index >= num_buses_ || to.index >= num_strips_)
        return PatchResult::OutOfRange;
    if (from.channel >= kChannels || to.channel >= kChannels)
        return PatchResult::OutOfRange;
    for (const PatchEntry& e : entries_)
        if (!e.removing && e.from == from && e.to == to) return PatchResult::Duplicate;
    return PatchResult::Ok;
}

void PatchBay::insert(const Endpoint& from, const Endpoint& to, float gain_db) {
    // Re-adding something mid-fade-out simply reverses the fade: cheaper than a
    // fade to silence followed by a fade back up, and it cannot click.
    for (PatchEntry& e : entries_) {
        if (e.from == from && e.to == to) {
            e.removing = false;
            e.gain_db = gain_db;
            e.gain.set_target(dsp::db_to_gain(gain_db), crossfade_frames_);
            return;
        }
    }
    PatchEntry e;
    e.from = from;
    e.to = to;
    e.gain_db = gain_db;
    e.gain.reset(0.0f);
    e.gain.set_target(dsp::db_to_gain(gain_db), crossfade_frames_);
    entries_.push_back(e);
}

bool PatchBay::begin_remove(const Endpoint& from, const Endpoint& to) noexcept {
    for (PatchEntry& e : entries_) {
        if (!e.removing && e.from == from && e.to == to) {
            e.removing = true;
            e.gain.set_target(0.0f, crossfade_frames_);
            return true;
        }
    }
    return false;
}

std::size_t PatchBay::prune() {
    const std::size_t before = entries_.size();
    entries_.erase(std::remove_if(entries_.begin(), entries_.end(),
                                  [](const PatchEntry& e) {
                                      return e.removing && e.gain.is_static() &&
                                             e.gain.value() == 0.0f;
                                  }),
                   entries_.end());
    return before - entries_.size();
}

void PatchBay::clear() noexcept { entries_.clear(); }

bool PatchBay::bus_feeds_strip(std::size_t bus, std::size_t strip) const noexcept {
    for (const PatchEntry& e : entries_) {
        // A patch on its way out still carries audio until it has faded, so it
        // still counts for loop detection.
        if (e.from.index == bus && e.to.index == strip) return true;
    }
    return false;
}

void PatchBay::snap() noexcept {
    for (PatchEntry& e : entries_) e.gain.reset(e.gain.target());
}

}  // namespace omni
