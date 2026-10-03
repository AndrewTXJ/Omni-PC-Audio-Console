#include "omni/control/command.hpp"

#include "omni/engine/graph.hpp"

namespace omni {

void apply(Graph& graph, const Command& c) noexcept {
    switch (c.type) {
        case CommandType::None:
            return;
        case CommandType::StripTrimDb:
            if (c.target < graph.num_strips()) graph.strip(c.target).set_trim_db(c.value);
            return;
        case CommandType::StripFaderDb:
            if (c.target < graph.num_strips()) graph.strip(c.target).set_fader_db(c.value);
            return;
        case CommandType::StripPan:
            if (c.target < graph.num_strips()) graph.strip(c.target).set_pan(c.value);
            return;
        case CommandType::StripMute:
            if (c.target < graph.num_strips()) graph.strip(c.target).set_mute(c.flag);
            return;
        case CommandType::StripPolarity:
            if (c.target < graph.num_strips())
                graph.strip(c.target).set_polarity_invert(c.flag);
            return;
        case CommandType::StripSendEnabled:
            if (c.target < graph.num_strips())
                graph.strip(c.target).set_send_enabled(c.index, c.flag);
            return;
        case CommandType::StripSendLevelDb:
            if (c.target < graph.num_strips())
                graph.strip(c.target).set_send_level_db(c.index, c.value);
            return;
        case CommandType::BusFaderDb:
            if (c.target < graph.num_buses()) graph.bus(c.target).set_fader_db(c.value);
            return;
        case CommandType::BusMute:
            if (c.target < graph.num_buses()) graph.bus(c.target).set_mute(c.flag);
            return;
        case CommandType::BusMono:
            if (c.target < graph.num_buses()) graph.bus(c.target).set_mono(c.flag);
            return;
        case CommandType::PanicMute:
            // 4.11: always visible, and it mutes everything that reaches a device.
            for (std::size_t b = 0; b < graph.num_buses(); ++b)
                graph.bus(b).set_mute(true);
            return;
        case CommandType::ClearMeters:
            for (std::size_t s = 0; s < graph.num_strips(); ++s) {
                graph.strip(s).input_meter().clear_clip();
                graph.strip(s).post_fader_meter().clear_clip();
            }
            for (std::size_t b = 0; b < graph.num_buses(); ++b) {
                graph.bus(b).meter().clear_clip();
                graph.bus(b).limiter().clear_engaged();
            }
            return;
    }
}

}  // namespace omni
