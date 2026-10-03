// Parameter changes crossing the UI -> audio thread boundary (roadmap 4.6:
// "parameters arrive over lock-free queues").
//
// This is the piece M1 deliberately left out, because with one thread there was
// nothing to put either side of it. A live device has two, so it exists now.
//
// A Command is a plain value: no pointers, no strings, no allocation. The audio
// thread drains the queue at the top of each callback and applies each command
// through the ordinary setters, so smoothing and ramps behave exactly as they do
// offline -- a fader moved from the UI ramps for the same 15 ms.
#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>

namespace omni {

class Graph;

enum class CommandType : std::uint8_t {
    None = 0,
    StripTrimDb,
    StripFaderDb,
    StripPan,
    StripMute,
    StripPolarity,
    StripSendEnabled,
    StripSendLevelDb,
    BusFaderDb,
    BusMute,
    BusMono,
    PanicMute,        // the always-visible panic mute (4.11)
    ClearMeters,
};

struct Command {
    CommandType type = CommandType::None;
    std::uint32_t target = 0;  // strip or bus index
    std::uint32_t index = 0;   // send index, where relevant
    float value = 0.0f;
    bool flag = false;
};

/// Apply a command to the graph. Audio thread; calls only the ordinary setters,
/// which allocate nothing.
void apply(Graph& graph, const Command& command) noexcept;

/// Single-producer, single-consumer ring buffer.
///
/// One writer (the UI thread) and one reader (the audio thread), and no more:
/// the indices are only safe for that pairing. The reader never blocks, never
/// allocates and never fails; the writer fails only when the queue is full,
/// which it reports rather than overwriting, because silently dropping a mute is
/// how a stream ends up live when the user pressed the button.
template <std::size_t Capacity>
class CommandQueue {
    static_assert(Capacity >= 2 && (Capacity & (Capacity - 1)) == 0,
                  "Capacity must be a power of two");

  public:
    /// Producer (UI thread). False if full.
    bool push(const Command& command) noexcept {
        const std::size_t write = write_.load(std::memory_order_relaxed);
        const std::size_t next = (write + 1) & kMask;
        if (next == read_.load(std::memory_order_acquire)) return false;  // full
        slots_[write] = command;
        write_.store(next, std::memory_order_release);
        return true;
    }

    /// Consumer (audio thread). False if empty.
    bool pop(Command& out) noexcept {
        const std::size_t read = read_.load(std::memory_order_relaxed);
        if (read == write_.load(std::memory_order_acquire)) return false;  // empty
        out = slots_[read];
        read_.store((read + 1) & kMask, std::memory_order_release);
        return true;
    }

    [[nodiscard]] bool empty() const noexcept {
        return read_.load(std::memory_order_acquire) ==
               write_.load(std::memory_order_acquire);
    }

    [[nodiscard]] std::size_t size() const noexcept {
        const std::size_t w = write_.load(std::memory_order_acquire);
        const std::size_t r = read_.load(std::memory_order_acquire);
        return (w - r) & kMask;
    }

    static constexpr std::size_t capacity() noexcept { return Capacity - 1; }

  private:
    static constexpr std::size_t kMask = Capacity - 1;
    std::array<Command, Capacity> slots_{};
    std::atomic<std::size_t> write_{0};
    std::atomic<std::size_t> read_{0};
};

static_assert(std::atomic<std::size_t>::is_always_lock_free,
              "the command queue must be lock-free on the audio thread");

/// 1024 slots: far more than a human can generate between two callbacks, and a
/// MIDI surface sweeping every fader still fits.
using DefaultCommandQueue = CommandQueue<1024>;

}  // namespace omni
