// The compiled graph (roadmap 4.1).
//
// "The graph runs as one flat pass inside the device callback." That is what
// this is: process() walks strips then buses in a fixed order, with no
// allocation, no locks, no syscalls and no logging. Every buffer is sized once
// by configure(), on the control thread.
//
// Buffer ownership, and one thing to know about it: strip_input() hands you the
// strip's own working buffer. process() CONSUMES it in place -- that is 4.6's
// "zero-copy, in-place processing", and it means a caller who needs the original
// samples afterwards (a null test, for instance) must keep its own copy.
//
// NOT YET HERE, and deliberately: parameter changes are applied by calling the
// setters between blocks. 4.6 requires them to arrive over lock-free queues
// instead, which becomes real when M2 introduces an actual audio thread; there
// is no point shipping an untested queue before there are two threads to put
// either side of it. Atomic graph rebuilds (4.1) land with the same milestone.
#pragma once

#include <cstddef>
#include <vector>

#include "omni/engine/bus.hpp"
#include "omni/engine/patch.hpp"
#include "omni/engine/strip.hpp"
#include "omni/engine/types.hpp"

namespace omni {

/// One entry in the compiled schedule.
struct ScheduleStep {
    enum class Kind : std::uint8_t { Strip, Bus };
    Kind kind = Kind::Strip;
    std::uint32_t index = 0;
};

struct GraphConfig {
    double sample_rate = 48000.0;
    std::size_t max_block = 1024;
    std::size_t num_strips = 0;
    std::size_t num_buses = 0;
};

class Graph {
  public:
    /// Control thread. Allocates everything the audio path will need.
    void configure(const GraphConfig& config);

    [[nodiscard]] std::size_t num_strips() const noexcept { return strips_.size(); }
    [[nodiscard]] std::size_t num_buses() const noexcept { return buses_.size(); }
    [[nodiscard]] double sample_rate() const noexcept { return config_.sample_rate; }
    [[nodiscard]] std::size_t max_block() const noexcept { return config_.max_block; }

    [[nodiscard]] Strip& strip(std::size_t i) noexcept { return strips_[i]; }
    [[nodiscard]] Bus& bus(std::size_t i) noexcept { return buses_[i]; }

    /// Write the source here before process(). Consumed in place.
    [[nodiscard]] float* strip_input(std::size_t strip, std::size_t channel) noexcept {
        return strip_ptrs_[strip * kChannels + channel];
    }
    /// Read after process().
    [[nodiscard]] const float* bus_output(std::size_t bus, std::size_t channel) const noexcept {
        return bus_ptrs_[bus * kChannels + channel];
    }

    /// Settle every ramp in the graph to its target, with no fade. Call after
    /// configuring an initial state and before the first process(); see
    /// Strip::snap() for why a live profile recall must not use it.
    void snap() noexcept {
        for (auto& s : strips_) s.snap();
        for (auto& b : buses_) b.snap();
        patches_.snap();
    }

    // ---- patching (4.2) -------------------------------------------------

    [[nodiscard]] PatchBay& patch_bay() noexcept { return patches_; }
    [[nodiscard]] const PatchBay& patch_bay() const noexcept { return patches_; }

    /// Control thread. Validates, rejects a patch that would create a feedback
    /// loop, then adds it and recompiles the schedule. `why` carries the
    /// explanation 4.2 requires the UI to show.
    PatchResult add_patch(const Endpoint& from, const Endpoint& to, float gain_db,
                          std::string& why);

    /// Control thread. Begins the fade-out; prune_patches() erases it once silent.
    bool remove_patch(const Endpoint& from, const Endpoint& to);

    /// Control thread, between blocks.
    std::size_t prune_patches();

    /// Would enabling this send create a loop? Lets the UI refuse an A/B button
    /// for the same reason it refuses a patch, rather than allowing a howl.
    [[nodiscard]] bool send_would_loop(std::size_t strip, std::size_t bus) const;

    /// Control thread. Recompute the processing order. False, with `why`, if the
    /// graph contains a cycle -- which add_patch and send_would_loop exist to
    /// prevent, so a false here means something bypassed them.
    bool compile(std::string& why);

    [[nodiscard]] const std::vector<ScheduleStep>& schedule() const noexcept {
        return schedule_;
    }

    /// Zero every strip input buffer. Convenience for callers between blocks;
    /// not called from process().
    void clear_inputs() noexcept;

    /// Audio thread: one flat pass. `frames` must be <= max_block().
    void process(std::size_t frames) noexcept;

  private:
    /// Kahn's algorithm over strips and buses, returning a processing order and
    /// false if the graph contains a cycle. Iterative, so a large graph cannot
    /// blow the stack.
    ///
    /// `extra_send_*` optionally adds one strip -> bus edge that is not in the
    /// graph yet, which is how send_would_loop() tests an A/B button before the
    /// user is allowed to press it. A prospective PATCH is tested differently --
    /// add_patch() inserts it and re-runs this, then rolls back -- because a patch
    /// has state (its crossfade ramp) that insert() owns.
    bool compute_order(std::vector<ScheduleStep>& out,
                       const std::uint32_t* extra_send_strip,
                       const std::uint32_t* extra_send_bus) const;

    GraphConfig config_{};
    std::vector<Strip> strips_;
    std::vector<Bus> buses_;
    PatchBay patches_;
    std::vector<ScheduleStep> schedule_;
    std::vector<float> strip_storage_;
    std::vector<float> bus_storage_;
    std::vector<float*> strip_ptrs_;
    std::vector<float*> bus_ptrs_;
};

}  // namespace omni
