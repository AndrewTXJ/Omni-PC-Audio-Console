// omni-render -- the offline renderer (roadmap 4.1).
//
// "The same engine renders faster than real time from files, so routing and DSP
// behaviour are testable in CI without a sound card." Same Graph, same strips
// and buses, same block-by-block processing a device callback will use; the only
// difference is where the samples come from.
//
// Usage:
//   omni-render --strip in.wav [--strip other.wav ...]
//               --route STRIP:BUS[:dB]
//               --bus BUS:out.wav[:fmt]
//               [--rate HZ] [--block N] [--frames N] [--verbose]
//
// Per-strip options (repeatable, applied to the most recent --strip):
//   --trim dB  --fader dB  --pan -1..1  --mute  --invert  --mono
// Per-bus options (applied to the most recent --bus):
//   --bus-fader dB  --bus-mute  --bus-mono  --ceiling dB
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "omni/dsp/db.hpp"
#include "omni/engine/graph.hpp"
#include "omni/io/wav.hpp"

namespace {

struct StripSpec {
    std::string path;
    float trim_db = 0.0f;
    float fader_db = 0.0f;
    float pan = 0.0f;
    bool mute = false;
    bool invert = false;
    bool mono = false;
    omni::io::AudioFile audio;
};

struct BusSpec {
    std::size_t index = 0;
    std::string path;
    omni::io::SampleFormat format = omni::io::SampleFormat::Float32;
    float fader_db = 0.0f;
    bool mute = false;
    bool mono = false;
    float ceiling_db = 0.0f;
};

struct RouteSpec {
    std::size_t strip = 0;
    std::size_t bus = 0;
    float level_db = 0.0f;
};

void usage() {
    std::fputs(
        "omni-render -- offline renderer for Omni-PC-Audio-Console\n"
        "\n"
        "  --strip FILE            add a strip fed by FILE (repeatable)\n"
        "    --trim dB             trim/Gain knob for the preceding strip (+/-24)\n"
        "    --fader dB            fader for the preceding strip (-inf..+12)\n"
        "    --pan P               -1 (left) .. +1 (right)\n"
        "    --mute / --invert     mute, or invert polarity\n"
        "    --mono                treat the source as mono (pan law applies)\n"
        "  --route S:B[:dB]        enable the send from strip S to bus B\n"
        "  --bus B:FILE[:fmt]      write bus B to FILE (fmt: f32|s16|s24|s32)\n"
        "    --bus-fader dB        fader for the preceding bus\n"
        "    --bus-mute --bus-mono\n"
        "    --ceiling dB          safety-limiter ceiling for the preceding bus\n"
        "  --rate HZ               project sample rate (default: first input's)\n"
        "  --block N               block size in frames (default 64)\n"
        "  --frames N              render N frames (default: longest input)\n"
        "  --verbose               report per-bus peak and limiter state\n",
        stderr);
}

bool split_colon(const std::string& s, std::vector<std::string>& parts) {
    parts.clear();
    std::size_t start = 0;
    while (true) {
        const std::size_t p = s.find(':', start);
        if (p == std::string::npos) {
            parts.push_back(s.substr(start));
            break;
        }
        parts.push_back(s.substr(start, p - start));
        start = p + 1;
    }
    return !parts.empty();
}

}  // namespace

int main(int argc, char** argv) {
    std::vector<StripSpec> strips;
    std::vector<BusSpec> buses;
    std::vector<RouteSpec> routes;
    double rate = 0.0;
    std::size_t block = 64;
    std::size_t frames_opt = 0;
    bool verbose = false;

    auto need = [&](int& i, const char* what) -> const char* {
        if (i + 1 >= argc) {
            std::fprintf(stderr, "omni-render: %s needs a value\n", what);
            std::exit(2);
        }
        return argv[++i];
    };

    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        if (a == "--help" || a == "-h") {
            usage();
            return 0;
        } else if (a == "--strip") {
            StripSpec s;
            s.path = need(i, "--strip");
            strips.push_back(std::move(s));
        } else if (a == "--trim" || a == "--fader" || a == "--pan" ||
                   a == "--mute" || a == "--invert" || a == "--mono") {
            if (strips.empty()) {
                std::fprintf(stderr, "omni-render: %s before any --strip\n", a.c_str());
                return 2;
            }
            StripSpec& s = strips.back();
            if (a == "--trim") s.trim_db = std::strtof(need(i, "--trim"), nullptr);
            else if (a == "--fader") s.fader_db = std::strtof(need(i, "--fader"), nullptr);
            else if (a == "--pan") s.pan = std::strtof(need(i, "--pan"), nullptr);
            else if (a == "--mute") s.mute = true;
            else if (a == "--invert") s.invert = true;
            else s.mono = true;
        } else if (a == "--route") {
            std::vector<std::string> p;
            split_colon(need(i, "--route"), p);
            if (p.size() < 2) {
                std::fputs("omni-render: --route wants STRIP:BUS[:dB]\n", stderr);
                return 2;
            }
            RouteSpec r;
            r.strip = static_cast<std::size_t>(std::strtoul(p[0].c_str(), nullptr, 10));
            r.bus = static_cast<std::size_t>(std::strtoul(p[1].c_str(), nullptr, 10));
            if (p.size() > 2) r.level_db = std::strtof(p[2].c_str(), nullptr);
            routes.push_back(r);
        } else if (a == "--bus") {
            std::vector<std::string> p;
            split_colon(need(i, "--bus"), p);
            if (p.size() < 2) {
                std::fputs("omni-render: --bus wants BUS:FILE[:fmt]\n", stderr);
                return 2;
            }
            BusSpec b;
            b.index = static_cast<std::size_t>(std::strtoul(p[0].c_str(), nullptr, 10));
            b.path = p[1];
            if (p.size() > 2 && !omni::io::parse_format(p[2], b.format)) {
                std::fprintf(stderr, "omni-render: unknown format '%s'\n", p[2].c_str());
                return 2;
            }
            buses.push_back(std::move(b));
        } else if (a == "--bus-fader" || a == "--bus-mute" || a == "--bus-mono" ||
                   a == "--ceiling") {
            if (buses.empty()) {
                std::fprintf(stderr, "omni-render: %s before any --bus\n", a.c_str());
                return 2;
            }
            BusSpec& b = buses.back();
            if (a == "--bus-fader") b.fader_db = std::strtof(need(i, "--bus-fader"), nullptr);
            else if (a == "--ceiling") b.ceiling_db = std::strtof(need(i, "--ceiling"), nullptr);
            else if (a == "--bus-mute") b.mute = true;
            else b.mono = true;
        } else if (a == "--rate") {
            rate = std::strtod(need(i, "--rate"), nullptr);
        } else if (a == "--block") {
            block = static_cast<std::size_t>(std::strtoul(need(i, "--block"), nullptr, 10));
        } else if (a == "--frames") {
            frames_opt = static_cast<std::size_t>(std::strtoul(need(i, "--frames"), nullptr, 10));
        } else if (a == "--verbose" || a == "-v") {
            verbose = true;
        } else {
            std::fprintf(stderr, "omni-render: unknown option '%s'\n", a.c_str());
            usage();
            return 2;
        }
    }

    if (strips.empty() || buses.empty()) {
        usage();
        return 2;
    }
    if (block == 0) {
        std::fputs("omni-render: --block must be positive\n", stderr);
        return 2;
    }

    // Load inputs.
    std::size_t longest = 0;
    for (StripSpec& s : strips) {
        std::string err;
        if (!omni::io::read_wav(s.path, s.audio, err)) {
            std::fprintf(stderr, "omni-render: %s\n", err.c_str());
            return 1;
        }
        longest = std::max(longest, s.audio.frames());
        if (rate == 0.0) rate = static_cast<double>(s.audio.sample_rate);
        if (static_cast<double>(s.audio.sample_rate) != rate) {
            // 7.6: a session has one project rate. Resampling at the boundary is
            // M4's job (clocking), so refuse rather than silently mis-render.
            std::fprintf(stderr,
                         "omni-render: %s is %u Hz but the project rate is %.0f Hz. "
                         "Resampling arrives with M4; convert the file first.\n",
                         s.path.c_str(), s.audio.sample_rate, rate);
            return 1;
        }
    }
    const std::size_t total = frames_opt ? frames_opt : longest;

    std::size_t num_buses = 0;
    for (const BusSpec& b : buses) num_buses = std::max(num_buses, b.index + 1);
    for (const RouteSpec& r : routes) num_buses = std::max(num_buses, r.bus + 1);

    omni::Graph graph;
    graph.configure({rate, block, strips.size(), num_buses});

    for (std::size_t i = 0; i < strips.size(); ++i) {
        const StripSpec& s = strips[i];
        omni::Strip& strip = graph.strip(i);
        strip.configure(rate, num_buses,
                        s.mono ? omni::ChannelMode::Mono : omni::ChannelMode::Stereo);
        strip.set_trim_db(s.trim_db);
        strip.set_fader_db(s.fader_db);
        strip.set_pan(s.pan);
        strip.set_mute(s.mute);
        strip.set_polarity_invert(s.invert);
    }
    for (const RouteSpec& r : routes) {
        if (r.strip >= strips.size()) {
            std::fprintf(stderr, "omni-render: --route names strip %zu; only %zu exist\n",
                         r.strip, strips.size());
            return 2;
        }
        graph.strip(r.strip).set_send_level_db(r.bus, r.level_db);
        graph.strip(r.strip).set_send_enabled(r.bus, true);
    }
    for (const BusSpec& b : buses) {
        omni::Bus& bus = graph.bus(b.index);
        bus.set_fader_db(b.fader_db);
        bus.set_mute(b.mute);
        bus.set_mono(b.mono);
        bus.limiter().set_ceiling(omni::dsp::db_to_gain(b.ceiling_db));
    }

    // Settle every ramp before frame 0. Without this the render opens with a
    // 7.5 ms fade-in as each send ramps up from silence, which is correct for a
    // device (principle 4: outputs start muted) and wrong for a file -- it made
    // the first 344 frames of an otherwise bit-exact render differ.
    graph.snap();

    // Collect output per bus.
    std::vector<omni::io::AudioFile> out(buses.size());
    for (std::size_t i = 0; i < buses.size(); ++i) {
        out[i].sample_rate = static_cast<std::uint32_t>(rate);
        out[i].channels = omni::kChannels;
        out[i].interleaved.reserve(total * omni::kChannels);
    }

    for (std::size_t off = 0; off < total; off += block) {
        const std::size_t n = std::min(block, total - off);
        graph.clear_inputs();
        for (std::size_t s = 0; s < strips.size(); ++s) {
            const omni::io::AudioFile& a = strips[s].audio;
            const std::size_t have = (off < a.frames()) ? std::min(n, a.frames() - off) : 0;
            float* l = graph.strip_input(s, 0);
            float* r = graph.strip_input(s, 1);
            for (std::size_t j = 0; j < have; ++j) {
                const std::size_t base = (off + j) * a.channels;
                l[j] = a.interleaved[base];
                r[j] = (a.channels > 1) ? a.interleaved[base + 1] : a.interleaved[base];
            }
        }
        graph.process(n);
        for (std::size_t i = 0; i < buses.size(); ++i) {
            const float* bl = graph.bus_output(buses[i].index, 0);
            const float* br = graph.bus_output(buses[i].index, 1);
            for (std::size_t j = 0; j < n; ++j) {
                out[i].interleaved.push_back(bl[j]);
                out[i].interleaved.push_back(br[j]);
            }
        }
    }

    for (std::size_t i = 0; i < buses.size(); ++i) {
        std::string err;
        if (!omni::io::write_wav(buses[i].path, out[i], buses[i].format, err)) {
            std::fprintf(stderr, "omni-render: %s\n", err.c_str());
            return 1;
        }
        if (verbose) {
            omni::Bus& bus = graph.bus(buses[i].index);
            const float peak = bus.meter().peek_peak();
            std::printf("bus %zu -> %s  (%s, %zu frames, peak %.2f dBFS%s%s)\n",
                        buses[i].index, buses[i].path.c_str(),
                        omni::io::format_name(buses[i].format), total,
                        static_cast<double>(omni::dsp::gain_to_db(peak)),
                        bus.meter().clipped() ? ", CLIPPED" : "",
                        bus.limiter().engaged() ? ", limiter engaged" : "");
        }
    }
    return 0;
}
