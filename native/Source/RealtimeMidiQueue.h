#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <algorithm>

namespace fengyin
{
struct RealtimeMidiEvent
{
    std::array<std::uint8_t, 3> bytes {};
    std::uint8_t size = 0;
    double timestampSeconds = 0.0;
};

// Single MIDI callback producer, single audio callback consumer. The storage is
// fixed at construction, so neither side allocates or waits for a lock.
template <std::size_t capacity>
class RealtimeMidiQueue final
{
public:
    static_assert(capacity >= 2);

    bool push(const std::uint8_t* bytes, int size, double timestampSeconds) noexcept
    {
        if (bytes == nullptr || size <= 0 || size > 3)
            return false;
        const auto write = writeIndex.load(std::memory_order_relaxed);
        const auto next = (write + 1) % capacity;
        if (next == readIndex.load(std::memory_order_acquire))
        {
            dropped.fetch_add(1, std::memory_order_relaxed);
            return false;
        }
        auto& event = events[write];
        std::copy_n(bytes, static_cast<std::size_t>(size), event.bytes.begin());
        event.size = static_cast<std::uint8_t>(size);
        event.timestampSeconds = timestampSeconds;
        writeIndex.store(next, std::memory_order_release);
        return true;
    }

    bool pop(RealtimeMidiEvent& event) noexcept
    {
        const auto read = readIndex.load(std::memory_order_relaxed);
        if (read == writeIndex.load(std::memory_order_acquire))
            return false;
        event = events[read];
        readIndex.store((read + 1) % capacity, std::memory_order_release);
        return true;
    }

    [[nodiscard]] std::uint64_t droppedCount() const noexcept
    {
        return dropped.load(std::memory_order_relaxed);
    }

private:
    std::array<RealtimeMidiEvent, capacity> events {};
    std::atomic<std::size_t> writeIndex { 0 };
    std::atomic<std::size_t> readIndex { 0 };
    std::atomic<std::uint64_t> dropped { 0 };
};
}
