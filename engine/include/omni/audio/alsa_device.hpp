// ALSA PCM device (roadmap 4.7, the "ALSA direct" backend; 4.6 for the rules).
//
// Phase 1 scope and honest limits:
//
//   * Blocking read/write on a dedicated thread, period-sized transfers. 4.6
//     ultimately wants mmap with hardware timestamps and wakeups just before the
//     period deadline; this is the simpler shape that works first, and the
//     interface below is what that optimisation goes behind.
//   * No device reservation yet. 4.7 requires acquiring the card politely through
//     org.freedesktop.ReserveDevice1 so PipeWire or Pulse release it. Until that
//     exists, opening a `hw:` device while a sound server holds it will simply
//     fail, and the error says so rather than fighting for it.
//   * Latency is read back from ALSA, not assumed. QS-02 compares it against a
//     raw loopback baseline, which needs hardware this container does not have.
//
// What IS verifiable without a sound card: the open/configure/transfer/recover
// path, against ALSA's userspace `null` PCM.
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace omni::audio {

struct DeviceInfo {
    std::string id;           ///< what to pass to open(), e.g. "default", "hw:0,0"
    std::string description;  ///< human-readable
};

/// Enumerate PCM devices. Playback and capture are listed separately because a
/// device can offer one and not the other.
[[nodiscard]] std::vector<DeviceInfo> list_devices(bool playback);

enum class Direction { Playback, Capture };

struct DeviceConfig {
    std::string id = "default";
    unsigned rate = 48000;
    unsigned channels = 2;
    std::size_t period_frames = 256;
    /// 4.6: "two periods where the device tolerates it, automatic fallback to
    /// three on xruns". Three is the safe default for a first open.
    unsigned periods = 3;
};

/// One direction of one PCM device. Not copyable: it owns a handle.
class AlsaDevice {
  public:
    AlsaDevice() = default;
    ~AlsaDevice();
    AlsaDevice(const AlsaDevice&) = delete;
    AlsaDevice& operator=(const AlsaDevice&) = delete;
    AlsaDevice(AlsaDevice&&) = delete;
    AlsaDevice& operator=(AlsaDevice&&) = delete;

    /// Control thread. Negotiates format, rate, channels and buffering, and
    /// reports what was actually granted -- which is often not what was asked.
    bool open(const DeviceConfig& config, Direction direction, std::string& error);
    void close() noexcept;
    [[nodiscard]] bool is_open() const noexcept { return handle_ != nullptr; }

    /// Audio thread. Interleaved float, `frames` per call. Recovers from xruns
    /// and counts them; returns false only on an unrecoverable error.
    bool write(const float* interleaved, std::size_t frames, std::string& error) noexcept;
    bool read(float* interleaved, std::size_t frames, std::string& error) noexcept;

    [[nodiscard]] unsigned rate() const noexcept { return rate_; }
    [[nodiscard]] unsigned channels() const noexcept { return channels_; }
    [[nodiscard]] std::size_t period_frames() const noexcept { return period_; }
    [[nodiscard]] unsigned periods() const noexcept { return periods_; }
    [[nodiscard]] const std::string& format_name() const noexcept { return format_name_; }
    [[nodiscard]] std::uint64_t xruns() const noexcept { return xruns_; }

    /// Buffer latency implied by the negotiated period and count, in ms. This is
    /// the engine's own contribution only -- converters, the bus and the driver's
    /// safety offset are not in it, so it is a floor, not a round trip (4.6).
    [[nodiscard]] double buffer_latency_ms() const noexcept;

  private:
    bool recover(int err, std::string& error) noexcept;

    void* handle_ = nullptr;            // snd_pcm_t*, opaque to keep ALSA out of headers
    int format_ = 0;                    // snd_pcm_format_t
    unsigned rate_ = 0;
    unsigned channels_ = 0;
    std::size_t period_ = 0;
    unsigned periods_ = 0;
    std::string format_name_;
    std::uint64_t xruns_ = 0;
    std::vector<unsigned char> scratch_;  // sized at open(); never grown in write()
};

}  // namespace omni::audio
