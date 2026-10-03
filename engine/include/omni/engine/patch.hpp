// The Patch window's wiring (roadmap 4.2, 3, and the Expert panel in 2.2).
//
// "Any channel can be anything. The 'hardware' and 'virtual' labels are only
// defaults... The Patch window wires individual channels" -- including
// many-to-one and one-to-many.
//
// Inside the graph the interesting patch is BUS OUTPUT -> STRIP INPUT, because
// that is the one that can create a feedback loop: strip -> bus via a send, bus
// -> strip via a patch, and the howl 4.2's loop protection and section 10 exist
// to prevent. Device endpoints are wired by the caller (the renderer, the mixer)
// and cannot loop on their own.
//
// Patches sum into a strip's input rather than replacing it, which is what makes
// many-to-one work: two buses patched to one strip mix.
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "omni/dsp/ramp.hpp"
#include "omni/engine/types.hpp"

namespace omni {

enum class EndpointKind : std::uint8_t { StripInput, BusOutput };

struct Endpoint {
    EndpointKind kind = EndpointKind::StripInput;
    std::uint32_t index = 0;    ///< strip or bus number
    std::uint32_t channel = 0;  ///< 0 = left, 1 = right

    friend bool operator==(const Endpoint& a, const Endpoint& b) {
        return a.kind == b.kind && a.index == b.index && a.channel == b.channel;
    }
};

/// Why a patch was refused. The UI shows the explanation 4.2 requires.
enum class PatchResult {
    Ok,
    OutOfRange,
    WrongDirection,
    Duplicate,
    WouldLoop,
};

[[nodiscard]] const char* patch_result_name(PatchResult r) noexcept;

struct PatchEntry {
    Endpoint from;  ///< a bus output
    Endpoint to;    ///< a strip input
    float gain_db = 0.0f;
    /// 4.2: "Patches crossfade over about 10 ms." A new patch fades in; a removed
    /// one fades out and is erased by prune() on the control thread, so a patch
    /// change never steps.
    dsp::Ramp gain{0.0f};
    bool removing = false;
};

/// Crossfade time for a patch change (4.2).
inline constexpr float kPatchCrossfadeMs = 10.0f;

class PatchBay {
  public:
    void configure(std::size_t num_strips, std::size_t num_buses, double sample_rate);

    /// Validation that does not need the send matrix. Loop detection lives in
    /// Graph, which is the only thing that knows both the patches and the sends.
    [[nodiscard]] PatchResult validate(const Endpoint& from, const Endpoint& to) const noexcept;

    /// Control thread. Assumes validate() and the caller's loop check passed.
    void insert(const Endpoint& from, const Endpoint& to, float gain_db);

    /// Control thread. Begins a fade-out; the entry survives until prune().
    bool begin_remove(const Endpoint& from, const Endpoint& to) noexcept;

    /// Control thread. Erase faded-out patches. Never call while the audio thread
    /// is in process().
    std::size_t prune();

    void clear() noexcept;

    [[nodiscard]] const std::vector<PatchEntry>& entries() const noexcept { return entries_; }
    [[nodiscard]] std::vector<PatchEntry>& entries() noexcept { return entries_; }

    /// True if any live patch carries bus -> strip. Used for loop detection.
    [[nodiscard]] bool bus_feeds_strip(std::size_t bus, std::size_t strip) const noexcept;

    /// Settle every patch ramp. Initial state only -- see Strip::snap().
    void snap() noexcept;

  private:
    std::vector<PatchEntry> entries_;
    std::size_t num_strips_ = 0;
    std::size_t num_buses_ = 0;
    std::size_t crossfade_frames_ = 0;
};

}  // namespace omni
