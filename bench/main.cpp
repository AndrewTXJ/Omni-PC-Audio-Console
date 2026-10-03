// omni-bench -- how much of the period deadline does the engine actually use?
//
// This is QS-09's measurement (roadmap 7.7: "64 channels at 48 kHz and 64 frames
// under 25% of one core on a mid-range CPU, without heavy FX"), and it needs no
// audio hardware, which matters because the only honest way to answer "can this
// run in real time" without a timed device is to measure the work and compare it
// against the deadline.
//
// Reported per the test plan's insistence: the MEAN is not the answer. A mean
// that passes while the worst case overruns the period is a failing engine, and
// the mean alone will not show it -- so the 99.9th percentile and the maximum
// are printed too, and the verdict uses the maximum.
//
// The absolute figure is a property of the machine. Publishing it requires a
// named part, which the test plan lists as an open question; this prints the
// numbers and leaves the naming to whoever runs it.
#include <time.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "omni/engine/graph.hpp"

namespace {

struct Series {
    double mean_us = 0.0;
    double p999_us = 0.0;
    double max_us = 0.0;
};

struct Result {
    Series cpu;     // thread CPU time: the engine's own work
    Series wall;    // elapsed time: includes being descheduled by the OS
    double deadline_us = 0.0;
};

/// Thread CPU time, which stops counting while the thread is not running. This
/// is what separates "the engine is slow" from "the machine preempted us" -- a
/// distinction a wall-clock-only benchmark cannot make, and which matters because
/// QS-09's target is a share of a core, not an absence of scheduler noise.
double thread_cpu_us() noexcept {
    timespec ts{};
    clock_gettime(CLOCK_THREAD_CPUTIME_ID, &ts);
    return static_cast<double>(ts.tv_sec) * 1e6 +
           static_cast<double>(ts.tv_nsec) / 1e3;
}

Series summarise(std::vector<double>& samples) {
    std::sort(samples.begin(), samples.end());
    Series s;
    double total = 0.0;
    for (const double v : samples) total += v;
    s.mean_us = total / static_cast<double>(samples.size());
    s.p999_us = samples[static_cast<std::size_t>(
        static_cast<double>(samples.size() - 1) * 0.999)];
    s.max_us = samples.back();
    return s;
}

Result run(std::size_t strips, std::size_t buses, std::size_t period, double rate,
           std::size_t blocks) {
    omni::Graph graph;
    graph.configure({rate, period, strips, buses});
    // Route everything to everything: the worst case for the send matrix, which
    // is the part that scales with strips x buses.
    for (std::size_t s = 0; s < strips; ++s)
        for (std::size_t b = 0; b < buses; ++b) graph.strip(s).set_send_enabled(b, true);
    graph.snap();

    // Fill with real signal, not silence: a zero buffer can be optimised in ways
    // a real one cannot, and a benchmark of zeros measures the wrong thing.
    for (std::size_t s = 0; s < strips; ++s) {
        for (std::size_t c = 0; c < omni::kChannels; ++c) {
            float* p = graph.strip_input(s, c);
            for (std::size_t i = 0; i < period; ++i)
                p[i] = 0.25f * std::sin(static_cast<float>(i + s) * 0.05f);
        }
    }

    std::vector<double> wall;
    std::vector<double> cpu;
    wall.reserve(blocks);
    cpu.reserve(blocks);

    // Warm up: first blocks pay for cold caches and are not representative.
    for (std::size_t i = 0; i < 256; ++i) graph.process(period);

    for (std::size_t i = 0; i < blocks; ++i) {
        const double c0 = thread_cpu_us();
        const auto t0 = std::chrono::steady_clock::now();
        graph.process(period);
        const auto t1 = std::chrono::steady_clock::now();
        const double c1 = thread_cpu_us();
        wall.push_back(std::chrono::duration<double, std::micro>(t1 - t0).count());
        cpu.push_back(c1 - c0);
    }

    Result r;
    r.cpu = summarise(cpu);
    r.wall = summarise(wall);
    r.deadline_us = (static_cast<double>(period) / rate) * 1e6;
    return r;
}

void report(const char* label, const Result& r) {
    std::printf("  %-30s deadline %7.1f us | CPU mean %6.2f (%4.1f%%) p99.9 %7.2f "
                "max %7.2f (%5.1f%%) | wall max %8.2f (%6.1f%%)\n",
                label, r.deadline_us, r.cpu.mean_us,
                100.0 * r.cpu.mean_us / r.deadline_us, r.cpu.p999_us, r.cpu.max_us,
                100.0 * r.cpu.max_us / r.deadline_us, r.wall.max_us,
                100.0 * r.wall.max_us / r.deadline_us);
}

}  // namespace

int main(int argc, char** argv) {
    double rate = 48000.0;
    std::size_t blocks = 20000;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        if ((a == "--rate") && i + 1 < argc) rate = std::strtod(argv[++i], nullptr);
        else if ((a == "--blocks") && i + 1 < argc)
            blocks = static_cast<std::size_t>(std::strtoul(argv[++i], nullptr, 10));
        else if (a == "--help") {
            std::fputs("omni-bench [--rate HZ] [--blocks N]\n", stderr);
            return 0;
        }
    }

    std::printf("omni-bench  %.0f Hz, %zu blocks per case\n\n", rate, blocks);

    std::printf("QS-09 target configuration (roadmap 7.7)\n");
    // 64 channels = 32 stereo strips. 8 buses, fully cross-routed.
    const Result qs09 = run(32, 8, 64, rate, blocks);
    report("64 ch / 8 buses / 64 frames", qs09);
    const double cpu_pct = 100.0 * qs09.cpu.max_us / qs09.deadline_us;
    const double wall_pct = 100.0 * qs09.wall.max_us / qs09.deadline_us;
    const bool pass = cpu_pct < 25.0;
    std::printf("\n  QS-09 verdict, worst-case CPU under 25%% of one core: %s "
                "[%.1f%% worst, %.1f%% mean]\n",
                pass ? "PASS" : "FAIL", cpu_pct,
                100.0 * qs09.cpu.mean_us / qs09.deadline_us);
    if (wall_pct >= 100.0) {
        std::printf("  Wall-clock worst case was %.0f%% of the deadline while CPU was "
                    "%.1f%%: this machine\n  descheduled the thread mid-block. On a "
                    "device that is an xrun, but the cause is system\n  tuning (4.6's "
                    "doctor: rtprio, memlock, governor, threadirqs), not engine work.\n",
                    wall_pct, cpu_pct);
    }
    std::printf("  The published figure needs a named CPU (test plan open "
                "question 2).\n\n");

    std::printf("Scaling\n");
    for (const std::size_t period : {32u, 64u, 128u, 256u, 512u}) {
        char label[64];
        std::snprintf(label, sizeof label, "64 ch / 8 buses / %zu frames", period);
        report(label, run(32, 8, period, rate, blocks));
    }
    std::printf("\n");
    for (const std::size_t strips : {1u, 4u, 8u, 16u, 32u, 64u}) {
        char label[64];
        std::snprintf(label, sizeof label, "%zu strips / 8 buses / 64 frames", strips);
        report(label, run(strips, 8, 64, rate, blocks));
    }
    return pass ? 0 : 1;
}
