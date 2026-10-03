#include "omni/io/wav.hpp"

#include <algorithm>
#include <cstring>
#include <fstream>

#include "omni/dsp/guards.hpp"

namespace omni::io {
namespace {

constexpr std::uint16_t kFormatPcm = 1;
constexpr std::uint16_t kFormatFloat = 3;
constexpr std::uint16_t kFormatExtensible = 0xFFFE;

std::uint16_t rd16(const unsigned char* p) noexcept {
    return static_cast<std::uint16_t>(static_cast<std::uint16_t>(p[0]) |
                                      static_cast<std::uint16_t>(p[1] << 8));
}

std::uint32_t rd32(const unsigned char* p) noexcept {
    return static_cast<std::uint32_t>(p[0]) |
           (static_cast<std::uint32_t>(p[1]) << 8) |
           (static_cast<std::uint32_t>(p[2]) << 16) |
           (static_cast<std::uint32_t>(p[3]) << 24);
}

void wr16(std::vector<unsigned char>& v, std::uint16_t x) {
    v.push_back(static_cast<unsigned char>(x & 0xFFu));
    v.push_back(static_cast<unsigned char>((x >> 8) & 0xFFu));
}

void wr32(std::vector<unsigned char>& v, std::uint32_t x) {
    v.push_back(static_cast<unsigned char>(x & 0xFFu));
    v.push_back(static_cast<unsigned char>((x >> 8) & 0xFFu));
    v.push_back(static_cast<unsigned char>((x >> 16) & 0xFFu));
    v.push_back(static_cast<unsigned char>((x >> 24) & 0xFFu));
}

void wrtag(std::vector<unsigned char>& v, const char* tag) {
    for (int i = 0; i < 4; ++i) v.push_back(static_cast<unsigned char>(tag[i]));
}

std::int32_t sign_extend_24(std::uint32_t raw) noexcept {
    if ((raw & 0x800000u) != 0u) raw |= 0xFF000000u;
    return static_cast<std::int32_t>(raw);
}

}  // namespace

const char* format_name(SampleFormat f) noexcept {
    switch (f) {
        case SampleFormat::Pcm16: return "s16";
        case SampleFormat::Pcm24: return "s24";
        case SampleFormat::Pcm32: return "s32";
        case SampleFormat::Float32: return "f32";
    }
    return "f32";
}

bool parse_format(const std::string& name, SampleFormat& out) noexcept {
    if (name == "s16") { out = SampleFormat::Pcm16; return true; }
    if (name == "s24") { out = SampleFormat::Pcm24; return true; }
    if (name == "s32") { out = SampleFormat::Pcm32; return true; }
    if (name == "f32" || name == "float") { out = SampleFormat::Float32; return true; }
    return false;
}

bool read_wav(const std::string& path, AudioFile& out, std::string& error) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        error = "cannot open " + path;
        return false;
    }
    std::vector<unsigned char> data((std::istreambuf_iterator<char>(file)),
                                    std::istreambuf_iterator<char>());
    if (data.size() < 12 || std::memcmp(data.data(), "RIFF", 4) != 0 ||
        std::memcmp(data.data() + 8, "WAVE", 4) != 0) {
        error = path + ": not a RIFF/WAVE file";
        return false;
    }

    std::uint16_t fmt = 0, channels = 0, bits = 0;
    std::uint32_t rate = 0;
    const unsigned char* audio = nullptr;
    std::size_t audio_bytes = 0;

    std::size_t pos = 12;
    while (pos + 8 <= data.size()) {
        const unsigned char* hdr = data.data() + pos;
        const std::uint32_t size = rd32(hdr + 4);
        const std::size_t body = pos + 8;
        if (std::memcmp(hdr, "fmt ", 4) == 0 && size >= 16 && body + 16 <= data.size()) {
            const unsigned char* f = data.data() + body;
            fmt = rd16(f);
            channels = rd16(f + 2);
            rate = rd32(f + 4);
            bits = rd16(f + 14);
            if (fmt == kFormatExtensible && size >= 26 && body + 26 <= data.size()) {
                fmt = rd16(f + 24);  // first two bytes of the SubFormat GUID
            }
        } else if (std::memcmp(hdr, "data", 4) == 0) {
            audio = data.data() + body;
            audio_bytes = std::min(static_cast<std::size_t>(size),
                                   data.size() - std::min(body, data.size()));
        }
        pos = body + size + (size & 1u);  // chunks are word-aligned
    }

    if (channels == 0 || rate == 0 || audio == nullptr) {
        error = path + ": missing fmt or data chunk";
        return false;
    }

    const std::size_t bytes_per_sample = bits / 8u;
    if (bytes_per_sample == 0) {
        error = path + ": zero bit depth";
        return false;
    }
    const std::size_t count = audio_bytes / bytes_per_sample;

    out.sample_rate = rate;
    out.channels = channels;
    out.interleaved.assign(count, 0.0f);

    if (fmt == kFormatFloat && bits == 32) {
        for (std::size_t i = 0; i < count; ++i) {
            std::uint32_t u = rd32(audio + i * 4);
            float v;
            std::memcpy(&v, &u, sizeof v);
            out.interleaved[i] = v;
        }
    } else if (fmt == kFormatPcm && bits == 16) {
        for (std::size_t i = 0; i < count; ++i)
            out.interleaved[i] =
                dsp::from_i16(static_cast<std::int16_t>(rd16(audio + i * 2)));
    } else if (fmt == kFormatPcm && bits == 24) {
        for (std::size_t i = 0; i < count; ++i) {
            const unsigned char* p = audio + i * 3;
            const std::uint32_t raw = static_cast<std::uint32_t>(p[0]) |
                                      (static_cast<std::uint32_t>(p[1]) << 8) |
                                      (static_cast<std::uint32_t>(p[2]) << 16);
            out.interleaved[i] = dsp::from_i24(sign_extend_24(raw));
        }
    } else if (fmt == kFormatPcm && bits == 32) {
        for (std::size_t i = 0; i < count; ++i)
            out.interleaved[i] =
                dsp::from_i32(static_cast<std::int32_t>(rd32(audio + i * 4)));
    } else {
        error = path + ": unsupported format (tag " + std::to_string(fmt) + ", " +
                std::to_string(bits) + " bit)";
        return false;
    }
    return true;
}

bool write_wav(const std::string& path, const AudioFile& in, SampleFormat format,
               std::string& error) {
    const std::uint16_t bits = (format == SampleFormat::Pcm16)   ? 16u
                               : (format == SampleFormat::Pcm24) ? 24u
                                                                 : 32u;
    const std::uint16_t tag =
        (format == SampleFormat::Float32) ? kFormatFloat : kFormatPcm;
    const std::uint16_t bytes_per_sample = static_cast<std::uint16_t>(bits / 8u);
    const std::uint16_t block_align =
        static_cast<std::uint16_t>(in.channels * bytes_per_sample);

    std::vector<unsigned char> body;
    body.reserve(in.interleaved.size() * bytes_per_sample);
    for (const float s : in.interleaved) {
        switch (format) {
            case SampleFormat::Float32: {
                std::uint32_t u;
                const float v = dsp::sanitize(s);
                std::memcpy(&u, &v, sizeof u);
                wr32(body, u);
                break;
            }
            case SampleFormat::Pcm16:
                wr16(body, static_cast<std::uint16_t>(dsp::to_i16(s)));
                break;
            case SampleFormat::Pcm24: {
                const std::uint32_t u = static_cast<std::uint32_t>(dsp::to_i24(s));
                body.push_back(static_cast<unsigned char>(u & 0xFFu));
                body.push_back(static_cast<unsigned char>((u >> 8) & 0xFFu));
                body.push_back(static_cast<unsigned char>((u >> 16) & 0xFFu));
                break;
            }
            case SampleFormat::Pcm32:
                wr32(body, static_cast<std::uint32_t>(dsp::to_i32(s)));
                break;
        }
    }

    std::vector<unsigned char> header;
    wrtag(header, "RIFF");
    wr32(header, static_cast<std::uint32_t>(36 + body.size()));
    wrtag(header, "WAVE");
    wrtag(header, "fmt ");
    wr32(header, 16);
    wr16(header, tag);
    wr16(header, in.channels);
    wr32(header, in.sample_rate);
    wr32(header, in.sample_rate * block_align);
    wr16(header, block_align);
    wr16(header, bits);
    wrtag(header, "data");
    wr32(header, static_cast<std::uint32_t>(body.size()));

    std::ofstream file(path, std::ios::binary);
    if (!file) {
        error = "cannot write " + path;
        return false;
    }
    file.write(reinterpret_cast<const char*>(header.data()),
               static_cast<std::streamsize>(header.size()));
    file.write(reinterpret_cast<const char*>(body.data()),
               static_cast<std::streamsize>(body.size()));
    if (!file) {
        error = "write failed for " + path;
        return false;
    }
    return true;
}

}  // namespace omni::io
