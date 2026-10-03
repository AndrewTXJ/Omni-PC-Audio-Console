// Minimal WAV reader/writer for the offline renderer (roadmap 4.1: "The same
// engine renders faster than real time from files, so routing and DSP behaviour
// are testable in CI without a sound card").
//
// Supports what 4.8 lists as the negotiated formats -- S16, S24, S32 -- plus
// 32-bit float, which is what an exactness test wants as its container.
// Deliberately not a general-purpose WAV library: no compressed formats, no
// chunk preservation.
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace omni::io {

enum class SampleFormat { Pcm16, Pcm24, Pcm32, Float32 };

struct AudioFile {
    std::uint32_t sample_rate = 48000;
    std::uint16_t channels = 2;
    std::vector<float> interleaved;

    [[nodiscard]] std::size_t frames() const noexcept {
        return channels == 0 ? 0 : interleaved.size() / channels;
    }
};

/// Read a WAV file. Returns false and sets `error` on failure.
bool read_wav(const std::string& path, AudioFile& out, std::string& error);

/// Write a WAV file in the given format.
bool write_wav(const std::string& path, const AudioFile& in, SampleFormat format,
               std::string& error);

[[nodiscard]] const char* format_name(SampleFormat f) noexcept;
[[nodiscard]] bool parse_format(const std::string& name, SampleFormat& out) noexcept;

}  // namespace omni::io
