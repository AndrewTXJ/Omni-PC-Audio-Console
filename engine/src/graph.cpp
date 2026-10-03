#include "omni/engine/graph.hpp"

#include <algorithm>

namespace omni {

void Graph::configure(const GraphConfig& config) {
    config_ = config;

    strips_.clear();
    strips_.resize(config.num_strips);
    buses_.clear();
    buses_.resize(config.num_buses);

    const std::size_t block = config.max_block;
    strip_storage_.assign(config.num_strips * kChannels * block, 0.0f);
    bus_storage_.assign(config.num_buses * kChannels * block, 0.0f);

    strip_ptrs_.resize(config.num_strips * kChannels);
    for (std::size_t s = 0; s < config.num_strips; ++s) {
        for (std::size_t c = 0; c < kChannels; ++c) {
            strip_ptrs_[s * kChannels + c] =
                strip_storage_.data() + (s * kChannels + c) * block;
        }
    }
    bus_ptrs_.resize(config.num_buses * kChannels);
    for (std::size_t b = 0; b < config.num_buses; ++b) {
        for (std::size_t c = 0; c < kChannels; ++c) {
            bus_ptrs_[b * kChannels + c] =
                bus_storage_.data() + (b * kChannels + c) * block;
        }
    }

    for (auto& s : strips_) s.configure(config.sample_rate, config.num_buses,
                                        ChannelMode::Stereo);
    for (auto& b : buses_) b.configure(config.sample_rate);
}

void Graph::clear_inputs() noexcept {
    std::fill(strip_storage_.begin(), strip_storage_.end(), 0.0f);
}

void Graph::process(std::size_t frames) noexcept {
    if (frames == 0 || frames > config_.max_block) return;

    // Zero the buses first. This is what makes an unpatched bus bit-exact
    // silence rather than stale audio (QS-04), and what makes a single unity
    // send reproduce its source exactly (0.0f + x == x).
    for (std::size_t b = 0; b < buses_.size(); ++b) {
        for (std::size_t c = 0; c < kChannels; ++c) {
            float* p = bus_ptrs_[b * kChannels + c];
            std::fill(p, p + frames, 0.0f);
        }
    }

    for (std::size_t s = 0; s < strips_.size(); ++s) {
        float* work[kChannels] = {strip_ptrs_[s * kChannels],
                                  strip_ptrs_[s * kChannels + 1]};
        strips_[s].process(work, frames, bus_ptrs_.data(), buses_.size());
    }

    for (std::size_t b = 0; b < buses_.size(); ++b) {
        float* bufs[kChannels] = {bus_ptrs_[b * kChannels],
                                  bus_ptrs_[b * kChannels + 1]};
        buses_[b].process(bufs, frames);
    }
}

}  // namespace omni
