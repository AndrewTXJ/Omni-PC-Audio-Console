// omni-mixer -- a live terminal mixer over ALSA.
//
// This is a CLIENT of the engine, which 4.1 makes the architecture: "The UI,
// tray, hotkeys, CLI and web remote are clients of one command and state API."
// The Qt 6/QML Simple view (2.1) is M3 and is not this; what this proves is that
// the engine runs in real time against a real device, with parameter changes
// crossing to the audio thread over the lock-free queue 4.6 requires.
//
// What it is not, so nobody is misled:
//   * No virtual devices. Apps cannot play into it yet -- that is M3 (4.2).
//   * No device reservation, so a `hw:` device held by PipeWire will refuse.
//   * Bus B1 is metered but goes nowhere: a B bus becomes a virtual capture
//     device at M3. The send matrix is real; its destination is not yet.
#include <alsa/asoundlib.h>
#include <poll.h>
#include <pthread.h>
#include <termios.h>
#include <unistd.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

#include "omni/audio/alsa_device.hpp"
#include "omni/control/command.hpp"
#include "omni/dsp/db.hpp"
#include "omni/engine/graph.hpp"

namespace {

constexpr std::size_t kNumBuses = 2;   // A1 -> device, B1 -> metered only (M3)

struct StripLabel {
    std::string name;
    bool is_tone = false;
};

struct App {
    omni::Graph graph;
    omni::DefaultCommandQueue queue;
    omni::audio::AlsaDevice out;
    omni::audio::AlsaDevice in;
    std::vector<StripLabel> labels;

    std::atomic<bool> running{true};
    std::atomic<bool> tone_on{true};
    std::atomic<double> tone_hz{1000.0};
    std::atomic<std::uint64_t> callbacks{0};
    std::atomic<std::uint64_t> dropped_commands{0};
    std::atomic<bool> rt_priority{false};
    std::atomic<bool> audio_failed{false};
    std::string audio_error;

    std::vector<float> io_out;   // interleaved device buffer
    std::vector<float> io_in;
};

App* g_app = nullptr;
termios g_saved_termios{};
bool g_termios_saved = false;

void restore_terminal() {
    if (g_termios_saved) {
        tcsetattr(STDIN_FILENO, TCSANOW, &g_saved_termios);
        g_termios_saved = false;
    }
    std::fputs("\x1b[?25h\x1b[0m\n", stdout);  // cursor back on
    std::fflush(stdout);
}

void on_signal(int) {
    if (g_app != nullptr) g_app->running.store(false);
}

bool enter_raw_mode() {
    if (!isatty(STDIN_FILENO)) return false;
    if (tcgetattr(STDIN_FILENO, &g_saved_termios) != 0) return false;
    g_termios_saved = true;
    termios raw = g_saved_termios;
    raw.c_lflag &= ~static_cast<tcflag_t>(ECHO | ICANON);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 0;
    return tcsetattr(STDIN_FILENO, TCSANOW, &raw) == 0;
}

// ---- the audio thread ---------------------------------------------------
//
// No allocation, no locks, no syscalls other than the PCM transfer itself, and
// no logging (4.6). Everything it needs was sized before it started.
void audio_thread(App& app) {
    sched_param param{};
    param.sched_priority = 70;
    if (pthread_setschedparam(pthread_self(), SCHED_FIFO, &param) == 0) {
        app.rt_priority.store(true);
    }

    const std::size_t period = app.out.period_frames();
    const unsigned out_ch = app.out.channels();
    const double rate = static_cast<double>(app.out.rate());
    const bool have_capture = app.in.is_open();
    const unsigned in_ch = have_capture ? app.in.channels() : 0;

    double phase = 0.0;
    std::string error;

    // Find the tone strip once, outside the loop.
    std::size_t tone_strip = app.labels.size();
    for (std::size_t i = 0; i < app.labels.size(); ++i)
        if (app.labels[i].is_tone) tone_strip = i;

    while (app.running.load(std::memory_order_relaxed)) {
        omni::Command command;
        while (app.queue.pop(command)) omni::apply(app.graph, command);

        app.graph.clear_inputs();

        if (have_capture) {
            if (!app.in.read(app.io_in.data(), period, error)) {
                app.audio_error = "capture: " + error;
                app.audio_failed.store(true);
                app.running.store(false);
                break;
            }
            float* l = app.graph.strip_input(0, 0);
            float* r = app.graph.strip_input(0, 1);
            for (std::size_t f = 0; f < period; ++f) {
                l[f] = app.io_in[f * in_ch];
                r[f] = (in_ch > 1) ? app.io_in[f * in_ch + 1] : app.io_in[f * in_ch];
            }
        }

        if (tone_strip < app.labels.size() && app.tone_on.load(std::memory_order_relaxed)) {
            const double hz = app.tone_hz.load(std::memory_order_relaxed);
            const double step = 2.0 * 3.14159265358979323846 * hz / rate;
            float* l = app.graph.strip_input(tone_strip, 0);
            float* r = app.graph.strip_input(tone_strip, 1);
            for (std::size_t f = 0; f < period; ++f) {
                const float v = static_cast<float>(0.25 * std::sin(phase));
                l[f] = v;
                r[f] = v;
                phase += step;
                if (phase > 6.283185307179586) phase -= 6.283185307179586;
            }
        }

        app.graph.process(period);

        const float* bl = app.graph.bus_output(0, 0);
        const float* br = app.graph.bus_output(0, 1);
        for (std::size_t f = 0; f < period; ++f) {
            for (unsigned c = 0; c < out_ch; ++c)
                app.io_out[f * out_ch + c] = (c == 0) ? bl[f] : br[f];
        }
        if (!app.out.write(app.io_out.data(), period, error)) {
            app.audio_error = "playback: " + error;
            app.audio_failed.store(true);
            app.running.store(false);
            break;
        }
        app.callbacks.fetch_add(1, std::memory_order_relaxed);
    }
}

// ---- the terminal UI ----------------------------------------------------

void send(App& app, const omni::Command& c) {
    if (!app.queue.push(c)) app.dropped_commands.fetch_add(1, std::memory_order_relaxed);
}

std::string meter_bar(float peak, int width) {
    const float db = omni::dsp::gain_to_db(peak);
    // -60 dBFS to 0 dBFS across the bar.
    int filled = 0;
    if (std::isfinite(db)) {
        const float frac = (db + 60.0f) / 60.0f;
        filled = static_cast<int>(frac * static_cast<float>(width));
    }
    if (filled < 0) filled = 0;
    if (filled > width) filled = width;
    std::string bar;
    bar.reserve(static_cast<std::size_t>(width) + 16);
    for (int i = 0; i < width; ++i) {
        if (i < filled) {
            // green to -12, amber to -3, red above
            const float at = (static_cast<float>(i) / static_cast<float>(width)) * 60.0f - 60.0f;
            bar += (at >= -3.0f) ? "\x1b[31m#" : (at >= -12.0f ? "\x1b[33m#" : "\x1b[32m#");
        } else {
            bar += "\x1b[90m.";
        }
    }
    bar += "\x1b[0m";
    return bar;
}

void draw(App& app, std::size_t selected) {
    std::string s;
    s.reserve(4096);
    s += "\x1b[H\x1b[2J";  // home, clear
    s += "\x1b[1mOmni-PC-Audio-Console\x1b[0m  \x1b[90m(omni-mixer: live terminal client, M2)\x1b[0m\n";

    char line[320];
    std::snprintf(line, sizeof line,
                  "out %s  %u Hz  %zu frames x %u  %s  buffer %.1f ms  xruns %llu\n",
                  app.out.format_name().c_str(), app.out.rate(),
                  app.out.period_frames(), app.out.periods(),
                  app.rt_priority.load() ? "\x1b[32mSCHED_FIFO\x1b[0m"
                                         : "\x1b[33mno RT priority\x1b[0m",
                  app.out.buffer_latency_ms(),
                  static_cast<unsigned long long>(app.out.xruns()));
    s += line;
    if (app.in.is_open()) {
        std::snprintf(line, sizeof line, "in  %s  %u Hz  %zu frames  xruns %llu\n",
                      app.in.format_name().c_str(), app.in.rate(),
                      app.in.period_frames(),
                      static_cast<unsigned long long>(app.in.xruns()));
        s += line;
    }
    std::snprintf(line, sizeof line, "blocks %llu   queue %zu\n\n",
                  static_cast<unsigned long long>(app.callbacks.load()),
                  app.queue.size());
    s += line;

    s += "\x1b[1m STRIPS\x1b[0m\n";
    for (std::size_t i = 0; i < app.graph.num_strips(); ++i) {
        omni::Strip& strip = app.graph.strip(i);
        const float peak = strip.post_fader_meter().read_peak();
        std::string sends;
        for (std::size_t b = 0; b < app.graph.num_buses(); ++b) {
            sends += (b == 0) ? "A1[" : "B1[";
            sends += strip.send_enabled(b) ? "x] " : " ] ";
        }
        std::snprintf(line, sizeof line, "%s %-8s fader %+6.1f dB  %s %s  %s\n",
                      (i == selected) ? "\x1b[7m>\x1b[0m" : " ",
                      app.labels[i].name.c_str(),
                      static_cast<double>(strip.fader_db()),
                      strip.muted() ? "\x1b[31mMUTE\x1b[0m" : "    ",
                      sends.c_str(), meter_bar(peak, 28).c_str());
        s += line;
        if (strip.post_fader_meter().clipped()) s += "   \x1b[31mCLIP\x1b[0m\n";
    }

    s += "\n\x1b[1m BUSES\x1b[0m\n";
    static const char* bus_names[kNumBuses] = {"A1 out", "B1"};
    for (std::size_t b = 0; b < app.graph.num_buses(); ++b) {
        omni::Bus& bus = app.graph.bus(b);
        const float peak = bus.meter().read_peak();
        std::snprintf(line, sizeof line, "  %-8s fader %+6.1f dB  %s %s %s%s\n",
                      bus_names[b], static_cast<double>(bus.fader_db()),
                      bus.muted() ? "\x1b[31mMUTE\x1b[0m" : "    ",
                      meter_bar(peak, 28).c_str(),
                      bus.limiter().engaged() ? " \x1b[33mLIM\x1b[0m" : "",
                      (b == 1) ? "  \x1b[90m(no virtual device yet - M3)\x1b[0m" : "");
        s += line;
    }

    s += "\n\x1b[90m j/k select   m mute   [ ] fader -/+1 dB   1 2 toggle A1/B1 send\n";
    s += " t tone on/off   - = tone freq   p PANIC mute all   c clear clips   q quit\x1b[0m\n";

    const std::uint64_t dropped = app.dropped_commands.load();
    if (dropped > 0) {
        std::snprintf(line, sizeof line,
                      "\x1b[31m %llu command(s) dropped (queue full)\x1b[0m\n",
                      static_cast<unsigned long long>(dropped));
        s += line;
    }
    std::fwrite(s.data(), 1, s.size(), stdout);
    std::fflush(stdout);
}

void usage() {
    std::fputs(
        "omni-mixer -- live terminal mixer (ALSA)\n"
        "\n"
        "  --list                 list playback and capture devices, then exit\n"
        "  --out DEVICE           playback device (default: \"default\")\n"
        "  --in DEVICE            optional capture device, becomes strip 0\n"
        "  --rate HZ              sample rate (default 48000)\n"
        "  --period FRAMES        period size (default 256)\n"
        "  --periods N            periods per buffer (default 3)\n"
        "  --seconds N            run headless for N seconds, then exit\n"
        "                         (no terminal UI; for scripted checks)\n",
        stderr);
}

}  // namespace

int main(int argc, char** argv) {
    std::string out_id = "default";
    std::string in_id;
    unsigned rate = 48000;
    std::size_t period = 256;
    unsigned periods = 3;
    double seconds = 0.0;

    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        auto next = [&](const char* what) -> const char* {
            if (i + 1 >= argc) {
                std::fprintf(stderr, "omni-mixer: %s needs a value\n", what);
                std::exit(2);
            }
            return argv[++i];
        };
        if (a == "--help" || a == "-h") { usage(); return 0; }
        else if (a == "--list") {
            for (const bool playback : {true, false}) {
                std::printf("%s:\n", playback ? "Playback devices" : "Capture devices");
                for (const auto& d : omni::audio::list_devices(playback))
                    std::printf("  %-28s %s\n", d.id.c_str(), d.description.c_str());
            }
            return 0;
        }
        else if (a == "--out") out_id = next("--out");
        else if (a == "--in") in_id = next("--in");
        else if (a == "--rate") rate = static_cast<unsigned>(std::strtoul(next("--rate"), nullptr, 10));
        else if (a == "--period") period = static_cast<std::size_t>(std::strtoul(next("--period"), nullptr, 10));
        else if (a == "--periods") periods = static_cast<unsigned>(std::strtoul(next("--periods"), nullptr, 10));
        else if (a == "--seconds") seconds = std::strtod(next("--seconds"), nullptr);
        else { std::fprintf(stderr, "omni-mixer: unknown option '%s'\n", a.c_str()); usage(); return 2; }
    }

    App app;
    g_app = &app;

    std::string error;
    omni::audio::DeviceConfig cfg;
    cfg.id = out_id;
    cfg.rate = rate;
    cfg.channels = 2;
    cfg.period_frames = period;
    cfg.periods = periods;
    if (!app.out.open(cfg, omni::audio::Direction::Playback, error)) {
        std::fprintf(stderr, "omni-mixer: %s\n", error.c_str());
        std::fputs("omni-mixer: try --list to see available devices\n", stderr);
        return 1;
    }
    if (!in_id.empty()) {
        omni::audio::DeviceConfig in_cfg = cfg;
        in_cfg.id = in_id;
        in_cfg.period_frames = app.out.period_frames();
        if (!app.in.open(in_cfg, omni::audio::Direction::Capture, error)) {
            std::fprintf(stderr, "omni-mixer: %s\n", error.c_str());
            return 1;
        }
        if (app.in.rate() != app.out.rate()) {
            // 7.6: one project rate per session. Resampling is M4.
            std::fprintf(stderr,
                         "omni-mixer: capture is %u Hz but playback is %u Hz. "
                         "Resampling arrives with M4; pick matching rates.\n",
                         app.in.rate(), app.out.rate());
            return 1;
        }
    }

    if (app.in.is_open()) app.labels.push_back({"Input", false});
    app.labels.push_back({"Tone", true});

    app.graph.configure({static_cast<double>(app.out.rate()), app.out.period_frames(),
                         app.labels.size(), kNumBuses});
    for (std::size_t i = 0; i < app.labels.size(); ++i) {
        // Principle 4: outputs start muted. The tone is audible, the mic is not,
        // so a first run cannot surprise anyone with feedback from their own
        // speakers.
        app.graph.strip(i).set_send_enabled(0, app.labels[i].is_tone);
        if (!app.labels[i].is_tone) app.graph.strip(i).set_mute(true);
    }
    app.graph.snap();

    app.io_out.assign(app.out.period_frames() * app.out.channels(), 0.0f);
    if (app.in.is_open())
        app.io_in.assign(app.in.period_frames() * app.in.channels(), 0.0f);

    std::signal(SIGINT, on_signal);
    std::signal(SIGTERM, on_signal);

    std::thread audio(audio_thread, std::ref(app));

    if (seconds > 0.0) {
        // Headless mode: no terminal, fixed duration. This is what makes the
        // backend checkable in a script, and against ALSA's `null` PCM it runs
        // with no sound card at all.
        const auto start = std::chrono::steady_clock::now();
        while (app.running.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            const double elapsed = std::chrono::duration<double>(
                                       std::chrono::steady_clock::now() - start).count();
            if (elapsed >= seconds) break;
        }
        app.running.store(false);
        audio.join();
        if (app.audio_failed.load()) {
            std::fprintf(stderr, "omni-mixer: %s\n", app.audio_error.c_str());
            return 1;
        }
        std::printf("ran %.1f s: %llu blocks of %zu frames at %u Hz, %llu xruns, "
                    "RT priority %s\n",
                    seconds, static_cast<unsigned long long>(app.callbacks.load()),
                    app.out.period_frames(), app.out.rate(),
                    static_cast<unsigned long long>(app.out.xruns()),
                    app.rt_priority.load() ? "yes" : "no");
        return 0;
    }

    const bool interactive = enter_raw_mode();
    if (!interactive) {
        std::fputs("omni-mixer: stdin is not a terminal; use --seconds N for a "
                   "headless run\n", stderr);
        app.running.store(false);
        audio.join();
        return 2;
    }
    std::atexit(restore_terminal);
    std::fputs("\x1b[?25l", stdout);  // hide cursor

    std::size_t selected = 0;
    while (app.running.load()) {
        draw(app, selected);

        pollfd pfd{STDIN_FILENO, POLLIN, 0};
        if (poll(&pfd, 1, 50) > 0) {
            char ch = 0;
            if (read(STDIN_FILENO, &ch, 1) == 1) {
                omni::Strip& strip = app.graph.strip(selected);
                switch (ch) {
                    case 'q': app.running.store(false); break;
                    case 'j': selected = (selected + 1) % app.graph.num_strips(); break;
                    case 'k': selected = (selected + app.graph.num_strips() - 1) %
                                         app.graph.num_strips(); break;
                    case 'm':
                        send(app, {omni::CommandType::StripMute,
                                   static_cast<std::uint32_t>(selected), 0, 0.0f,
                                   !strip.muted()});
                        break;
                    case '[':
                        send(app, {omni::CommandType::StripFaderDb,
                                   static_cast<std::uint32_t>(selected), 0,
                                   strip.fader_db() - 1.0f, false});
                        break;
                    case ']':
                        send(app, {omni::CommandType::StripFaderDb,
                                   static_cast<std::uint32_t>(selected), 0,
                                   strip.fader_db() + 1.0f, false});
                        break;
                    case '1':
                    case '2': {
                        const std::uint32_t bus = static_cast<std::uint32_t>(ch - '1');
                        if (bus < kNumBuses)
                            send(app, {omni::CommandType::StripSendEnabled,
                                       static_cast<std::uint32_t>(selected), bus, 0.0f,
                                       !strip.send_enabled(bus)});
                        break;
                    }
                    case 't': app.tone_on.store(!app.tone_on.load()); break;
                    case '-': app.tone_hz.store(std::max(50.0, app.tone_hz.load() / 1.26)); break;
                    case '=': app.tone_hz.store(std::min(16000.0, app.tone_hz.load() * 1.26)); break;
                    case 'p': send(app, {omni::CommandType::PanicMute, 0, 0, 0.0f, true}); break;
                    case 'c': send(app, {omni::CommandType::ClearMeters, 0, 0, 0.0f, false}); break;
                    default: break;
                }
            }
        }
    }

    audio.join();
    restore_terminal();
    if (app.audio_failed.load()) {
        std::fprintf(stderr, "omni-mixer: %s\n", app.audio_error.c_str());
        return 1;
    }
    std::printf("stopped after %llu blocks, %llu xruns\n",
                static_cast<unsigned long long>(app.callbacks.load()),
                static_cast<unsigned long long>(app.out.xruns()));
    return 0;
}
