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
#include "omni/engine/strip.hpp"
#include "omni/engine/types.hpp"

namespace omni {

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
    }

    /// Zero every strip input buffer. Convenience for callers between blocks;
    /// not called from process().
    void clear_inputs() noexcept;

    /// Audio thread: one flat pass. `frames` must be <= max_block().
    void process(std::size_t frames) noexcept;

  private:
    GraphConfig config_{};
    std::vector<Strip> strips_;
    std::vector<Bus> buses_;
    std::vector<float> strip_storage_;
    std::vector<float> bus_storage_;
    std::vector<float*> strip_ptrs_;
    std::vector<float*> bus_ptrs_;
};

}  // namespace omni
