#include "omni/audio/alsa_device.hpp"

#include <alsa/asoundlib.h>

#include <algorithm>
#include <cstring>

#include "omni/dsp/guards.hpp"

namespace omni::audio {
namespace {

snd_pcm_t* as_pcm(void* h) noexcept { return static_cast<snd_pcm_t*>(h); }

struct FormatChoice {
    snd_pcm_format_t format;
    const char* name;
    std::size_t bytes;
};

// Float first: it is what the engine already holds, so there is no conversion
// and nothing to round. The integer fallbacks go through the saturating
// converters in guards.hpp, which is also what 4.1 requires of them.
constexpr FormatChoice kFormats[] = {
    {SND_PCM_FORMAT_FLOAT_LE, "f32", 4},
    {SND_PCM_FORMAT_S32_LE, "s32", 4},
    {SND_PCM_FORMAT_S24_3LE, "s24_3", 3},
    {SND_PCM_FORMAT_S16_LE, "s16", 2},
};

std::size_t format_bytes(int f) noexcept {
    for (const FormatChoice& c : kFormats)
        if (static_cast<int>(c.format) == f) return c.bytes;
    return 4;
}

}  // namespace

std::vector<DeviceInfo> list_devices(bool playback) {
    std::vector<DeviceInfo> out;
    void** hints = nullptr;
    if (snd_device_name_hint(-1, "pcm", &hints) != 0 || hints == nullptr) return out;

    const char* want = playback ? "Output" : "Input";
    for (void** h = hints; *h != nullptr; ++h) {
        char* name = snd_device_name_get_hint(*h, "NAME");
        char* desc = snd_device_name_get_hint(*h, "DESC");
        char* ioid = snd_device_name_get_hint(*h, "IOID");
        // IOID null means the device does both directions.
        const bool matches = (ioid == nullptr) || (std::strcmp(ioid, want) == 0);
        if (name != nullptr && matches) {
            DeviceInfo info;
            info.id = name;
            if (desc != nullptr) {
                info.description = desc;
                // Descriptions are multi-line; keep the first line for a list.
                const std::size_t nl = info.description.find('\n');
                if (nl != std::string::npos) info.description.resize(nl);
            }
            out.push_back(std::move(info));
        }
        std::free(name);
        std::free(desc);
        std::free(ioid);
    }
    snd_device_name_free_hint(hints);
    return out;
}

AlsaDevice::~AlsaDevice() { close(); }

void AlsaDevice::close() noexcept {
    if (handle_ != nullptr) {
        snd_pcm_drop(as_pcm(handle_));
        snd_pcm_close(as_pcm(handle_));
        handle_ = nullptr;
    }
}

bool AlsaDevice::open(const DeviceConfig& config, Direction direction,
                      std::string& error) {
    close();

    snd_pcm_t* pcm = nullptr;
    const snd_pcm_stream_t stream =
        (direction == Direction::Playback) ? SND_PCM_STREAM_PLAYBACK
                                           : SND_PCM_STREAM_CAPTURE;
    int rc = snd_pcm_open(&pcm, config.id.c_str(), stream, 0);
    if (rc < 0) {
        error = "cannot open '" + config.id + "': " + snd_strerror(rc);
        if (rc == -EBUSY) {
            error += " (another client holds it; device reservation via "
                     "ReserveDevice1 is not implemented yet -- roadmap 4.7)";
        }
        return false;
    }

    snd_pcm_hw_params_t* hw = nullptr;
    snd_pcm_hw_params_alloca(&hw);
    snd_pcm_hw_params_any(pcm, hw);

    rc = snd_pcm_hw_params_set_access(pcm, hw, SND_PCM_ACCESS_RW_INTERLEAVED);
    if (rc < 0) {
        error = std::string("interleaved access unavailable: ") + snd_strerror(rc);
        snd_pcm_close(pcm);
        return false;
    }

    bool chose = false;
    for (const FormatChoice& c : kFormats) {
        if (snd_pcm_hw_params_set_format(pcm, hw, c.format) >= 0) {
            format_ = static_cast<int>(c.format);
            format_name_ = c.name;
            chose = true;
            break;
        }
    }
    if (!chose) {
        error = "device supports none of f32, s32, s24_3, s16";
        snd_pcm_close(pcm);
        return false;
    }

    unsigned channels = config.channels;
    rc = snd_pcm_hw_params_set_channels_near(pcm, hw, &channels);
    if (rc < 0) {
        error = std::string("cannot set channels: ") + snd_strerror(rc);
        snd_pcm_close(pcm);
        return false;
    }

    unsigned rate = config.rate;
    rc = snd_pcm_hw_params_set_rate_near(pcm, hw, &rate, nullptr);
    if (rc < 0) {
        error = std::string("cannot set rate: ") + snd_strerror(rc);
        snd_pcm_close(pcm);
        return false;
    }

    snd_pcm_uframes_t period = config.period_frames;
    rc = snd_pcm_hw_params_set_period_size_near(pcm, hw, &period, nullptr);
    if (rc < 0) {
        error = std::string("cannot set period size: ") + snd_strerror(rc);
        snd_pcm_close(pcm);
        return false;
    }
    unsigned periods = config.periods;
    snd_pcm_hw_params_set_periods_near(pcm, hw, &periods, nullptr);

    rc = snd_pcm_hw_params(pcm, hw);
    if (rc < 0) {
        error = std::string("cannot apply hw params: ") + snd_strerror(rc);
        snd_pcm_close(pcm);
        return false;
    }

    // Report what was granted, not what was requested.
    snd_pcm_hw_params_get_period_size(hw, &period, nullptr);
    snd_pcm_hw_params_get_periods(hw, &periods, nullptr);
    snd_pcm_hw_params_get_rate(hw, &rate, nullptr);
    snd_pcm_hw_params_get_channels(hw, &channels);

    rc = snd_pcm_prepare(pcm);
    if (rc < 0) {
        error = std::string("cannot prepare: ") + snd_strerror(rc);
        snd_pcm_close(pcm);
        return false;
    }

    handle_ = pcm;
    rate_ = rate;
    channels_ = channels;
    period_ = period;
    periods_ = periods;
    xruns_ = 0;
    // Sized once, here. write() and read() must not allocate (4.6).
    scratch_.assign(period_ * channels_ * format_bytes(format_), 0);
    return true;
}

double AlsaDevice::buffer_latency_ms() const noexcept {
    if (rate_ == 0) return 0.0;
    return (static_cast<double>(period_) * static_cast<double>(periods_) * 1000.0) /
           static_cast<double>(rate_);
}

bool AlsaDevice::recover(int err, std::string& error) noexcept {
    if (err == -EPIPE || err == -ESTRPIPE) ++xruns_;
    const int rc = snd_pcm_recover(as_pcm(handle_), err, 1 /* silent */);
    if (rc < 0) {
        error = std::string("unrecoverable PCM error: ") + snd_strerror(rc);
        return false;
    }
    return true;
}

bool AlsaDevice::write(const float* interleaved, std::size_t frames,
                       std::string& error) noexcept {
    if (handle_ == nullptr) {
        error = "device not open";
        return false;
    }
    const std::size_t samples = frames * channels_;
    const void* src = interleaved;

    if (format_ != static_cast<int>(SND_PCM_FORMAT_FLOAT_LE)) {
        unsigned char* dst = scratch_.data();
        if (format_ == static_cast<int>(SND_PCM_FORMAT_S32_LE)) {
            for (std::size_t i = 0; i < samples; ++i) {
                const std::int32_t v = dsp::to_i32(interleaved[i]);
                std::memcpy(dst + i * 4, &v, 4);
            }
        } else if (format_ == static_cast<int>(SND_PCM_FORMAT_S24_3LE)) {
            for (std::size_t i = 0; i < samples; ++i) {
                const std::int32_t v = dsp::to_i24(interleaved[i]);
                dst[i * 3 + 0] = static_cast<unsigned char>(v & 0xFF);
                dst[i * 3 + 1] = static_cast<unsigned char>((v >> 8) & 0xFF);
                dst[i * 3 + 2] = static_cast<unsigned char>((v >> 16) & 0xFF);
            }
        } else {
            for (std::size_t i = 0; i < samples; ++i) {
                const std::int16_t v = dsp::to_i16(interleaved[i]);
                std::memcpy(dst + i * 2, &v, 2);
            }
        }
        src = dst;
    }

    const unsigned char* cursor = static_cast<const unsigned char*>(src);
    std::size_t left = frames;
    const std::size_t frame_bytes = channels_ * format_bytes(format_);
    while (left > 0) {
        const snd_pcm_sframes_t done = snd_pcm_writei(as_pcm(handle_), cursor, left);
        if (done < 0) {
            if (!recover(static_cast<int>(done), error)) return false;
            continue;
        }
        cursor += static_cast<std::size_t>(done) * frame_bytes;
        left -= static_cast<std::size_t>(done);
    }
    return true;
}

bool AlsaDevice::read(float* interleaved, std::size_t frames,
                      std::string& error) noexcept {
    if (handle_ == nullptr) {
        error = "device not open";
        return false;
    }
    const bool direct = format_ == static_cast<int>(SND_PCM_FORMAT_FLOAT_LE);
    unsigned char* dst = direct ? reinterpret_cast<unsigned char*>(interleaved)
                                : scratch_.data();
    const std::size_t frame_bytes = channels_ * format_bytes(format_);

    unsigned char* cursor = dst;
    std::size_t left = frames;
    while (left > 0) {
        const snd_pcm_sframes_t done = snd_pcm_readi(as_pcm(handle_), cursor, left);
        if (done < 0) {
            if (!recover(static_cast<int>(done), error)) return false;
            continue;
        }
        cursor += static_cast<std::size_t>(done) * frame_bytes;
        left -= static_cast<std::size_t>(done);
    }

    if (!direct) {
        const std::size_t samples = frames * channels_;
        if (format_ == static_cast<int>(SND_PCM_FORMAT_S32_LE)) {
            for (std::size_t i = 0; i < samples; ++i) {
                std::int32_t v;
                std::memcpy(&v, dst + i * 4, 4);
                interleaved[i] = dsp::from_i32(v);
            }
        } else if (format_ == static_cast<int>(SND_PCM_FORMAT_S24_3LE)) {
            for (std::size_t i = 0; i < samples; ++i) {
                std::uint32_t raw = static_cast<std::uint32_t>(dst[i * 3 + 0]) |
                                    (static_cast<std::uint32_t>(dst[i * 3 + 1]) << 8) |
                                    (static_cast<std::uint32_t>(dst[i * 3 + 2]) << 16);
                if ((raw & 0x800000u) != 0u) raw |= 0xFF000000u;
                interleaved[i] = dsp::from_i24(static_cast<std::int32_t>(raw));
            }
        } else {
            for (std::size_t i = 0; i < samples; ++i) {
                std::int16_t v;
                std::memcpy(&v, dst + i * 2, 2);
                interleaved[i] = dsp::from_i16(v);
            }
        }
    }
    return true;
}

}  // namespace omni::audio
