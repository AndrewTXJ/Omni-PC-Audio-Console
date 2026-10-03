#include "omni/engine/graph.hpp"

#include <algorithm>
#include <deque>

#include "omni/dsp/ramp.hpp"

namespace omni {
namespace {

/// Mix one channel of a bus output into one channel of a strip input, through the
/// patch's crossfade ramp. Patches SUM, which is what makes many-to-one work.
void mix_patch(const float* src, float* dst, std::size_t n, dsp::Ramp& ramp) noexcept {
    if (ramp.is_static()) {
        const float g = ramp.value();
        if (g == 0.0f) return;
        if (g == 1.0f) {
            for (std::size_t i = 0; i < n; ++i) dst[i] += src[i];
            return;
        }
        for (std::size_t i = 0; i < n; ++i) dst[i] += src[i] * g;
        return;
    }
    for (std::size_t i = 0; i < n; ++i) dst[i] += src[i] * ramp.next();
}

}  // namespace

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
    for (std::size_t s = 0; s < config.num_strips; ++s)
        for (std::size_t c = 0; c < kChannels; ++c)
            strip_ptrs_[s * kChannels + c] =
                strip_storage_.data() + (s * kChannels + c) * block;

    bus_ptrs_.resize(config.num_buses * kChannels);
    for (std::size_t b = 0; b < config.num_buses; ++b)
        for (std::size_t c = 0; c < kChannels; ++c)
            bus_ptrs_[b * kChannels + c] =
                bus_storage_.data() + (b * kChannels + c) * block;

    for (auto& s : strips_)
        s.configure(config.sample_rate, config.num_buses, ChannelMode::Stereo);
    for (auto& b : buses_) b.configure(config.sample_rate);

    patches_.configure(config.num_strips, config.num_buses, config.sample_rate);

    std::string why;
    compile(why);  // cannot fail with no patches
}

bool Graph::compute_order(std::vector<ScheduleStep>& out,
                          const std::uint32_t* extra_send_strip,
                          const std::uint32_t* extra_send_bus) const {
    const std::size_t ns = strips_.size();
    const std::size_t nb = buses_.size();
    const std::size_t n = ns + nb;
    out.clear();
    if (n == 0) return true;

    // Node ids: strips 0..ns-1, buses ns..ns+nb-1.
    const auto bus_node = [ns](std::size_t b) { return ns + b; };

    std::vector<std::vector<std::size_t>> succ(n);
    std::vector<std::size_t> indegree(n, 0);

    const auto add_edge = [&](std::size_t a, std::size_t b) {
        succ[a].push_back(b);
        ++indegree[b];
    };

    // strip -> bus for every send that carries audio.
    for (std::size_t s = 0; s < ns; ++s)
        for (std::size_t b = 0; b < nb; ++b)
            if (strips_[s].send_active(b)) add_edge(s, bus_node(b));

    // bus -> strip for every patch.
    for (const PatchEntry& e : patches_.entries())
        if (e.from.index < nb && e.to.index < ns)
            add_edge(bus_node(e.from.index), e.to.index);

    // The prospective send under test, if any.
    if (extra_send_strip != nullptr && extra_send_bus != nullptr &&
        *extra_send_strip < ns && *extra_send_bus < nb)
        add_edge(*extra_send_strip, bus_node(*extra_send_bus));

    std::deque<std::size_t> ready;
    for (std::size_t i = 0; i < n; ++i)
        if (indegree[i] == 0) ready.push_back(i);

    out.reserve(n);
    while (!ready.empty()) {
        const std::size_t node = ready.front();
        ready.pop_front();
        ScheduleStep step;
        if (node < ns) {
            step.kind = ScheduleStep::Kind::Strip;
            step.index = static_cast<std::uint32_t>(node);
        } else {
            step.kind = ScheduleStep::Kind::Bus;
            step.index = static_cast<std::uint32_t>(node - ns);
        }
        out.push_back(step);
        for (const std::size_t next : succ[node])
            if (--indegree[next] == 0) ready.push_back(next);
    }

    // Kahn's algorithm leaves nodes unvisited exactly when there is a cycle.
    return out.size() == n;
}

bool Graph::compile(std::string& why) {
    std::vector<ScheduleStep> order;
    if (!compute_order(order, nullptr, nullptr)) {
        why = "the graph contains a feedback loop";
        return false;
    }
    schedule_ = std::move(order);
    return true;
}

bool Graph::send_would_loop(std::size_t strip, std::size_t bus) const {
    if (strip >= strips_.size() || bus >= buses_.size()) return false;
    if (strips_[strip].send_active(bus)) return false;  // already there
    const std::uint32_t s = static_cast<std::uint32_t>(strip);
    const std::uint32_t b = static_cast<std::uint32_t>(bus);
    std::vector<ScheduleStep> scratch;
    return !compute_order(scratch, &s, &b);
}

PatchResult Graph::add_patch(const Endpoint& from, const Endpoint& to, float gain_db,
                             std::string& why) {
    const PatchResult basic = patches_.validate(from, to);
    if (basic != PatchResult::Ok) {
        why = patch_result_name(basic);
        return basic;
    }

    // Test the edge before committing it: a patch that closes a cycle is exactly
    // the "loud howl" risk 4.2's loop protection and section 10 are about.
    patches_.insert(from, to, gain_db);
    std::vector<ScheduleStep> order;
    const bool ok = compute_order(order, nullptr, nullptr);
    if (!ok) {
        patches_.begin_remove(from, to);
        // Drop it outright: it never carried audio, so there is nothing to fade.
        for (PatchEntry& e : patches_.entries())
            if (e.from == from && e.to == to) e.gain.reset(0.0f);
        patches_.prune();
        why = std::string(patch_result_name(PatchResult::WouldLoop)) +
              ": bus " + std::to_string(from.index) + " already receives audio that " +
              "would come back through strip " + std::to_string(to.index);
        return PatchResult::WouldLoop;
    }
    schedule_ = std::move(order);
    why.clear();
    return PatchResult::Ok;
}

bool Graph::remove_patch(const Endpoint& from, const Endpoint& to) {
    return patches_.begin_remove(from, to);
}

std::size_t Graph::prune_patches() {
    const std::size_t removed = patches_.prune();
    if (removed > 0) {
        std::string why;
        compile(why);  // removing an edge cannot create a cycle
    }
    return removed;
}

void Graph::clear_inputs() noexcept {
    std::fill(strip_storage_.begin(), strip_storage_.end(), 0.0f);
}

void Graph::process(std::size_t frames) noexcept {
    if (frames == 0 || frames > config_.max_block) return;

    // Zero the buses first. This is what makes an unpatched bus bit-exact silence
    // rather than stale audio (QS-04), and what makes a single unity send
    // reproduce its source exactly (0.0f + x == x).
    for (std::size_t b = 0; b < buses_.size(); ++b)
        for (std::size_t c = 0; c < kChannels; ++c) {
            float* p = bus_ptrs_[b * kChannels + c];
            std::fill(p, p + frames, 0.0f);
        }

    // One flat pass in dependency order (4.1). The schedule guarantees that every
    // bus feeding a strip through a patch, and every strip feeding a bus through a
    // send, has already run when it is needed.
    for (const ScheduleStep& step : schedule_) {
        if (step.kind == ScheduleStep::Kind::Strip) {
            const std::size_t s = step.index;
            // Incoming patches sum into the strip's input alongside whatever the
            // caller wrote there.
            for (PatchEntry& e : patches_.entries()) {
                if (e.to.index != s) continue;
                const float* src = bus_ptrs_[e.from.index * kChannels + e.from.channel];
                float* dst = strip_ptrs_[s * kChannels + e.to.channel];
                mix_patch(src, dst, frames, e.gain);
            }
            float* work[kChannels] = {strip_ptrs_[s * kChannels],
                                      strip_ptrs_[s * kChannels + 1]};
            strips_[s].process(work, frames, bus_ptrs_.data(), buses_.size());
        } else {
            const std::size_t b = step.index;
            float* bufs[kChannels] = {bus_ptrs_[b * kChannels],
                                      bus_ptrs_[b * kChannels + 1]};
            buses_[b].process(bufs, frames);
        }
    }
}

}  // namespace omni
