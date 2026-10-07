#pragma once
#include "RealtimeMidiQueue.h"

namespace fengyin
{
// Bounded multi-producer/single-consumer queue. MIDI-derived technique commands
// and UI commands must not share an SPSC write index or be drained separately.
template <std::size_t capacity>
class OrderedMidiQueue final
{
    struct Slot
    {
        std::atomic<std::size_t> sequence { 0 };
        RealtimeMidiEvent event;
    };
public:
    OrderedMidiQueue() noexcept
    {
        for (std::size_t i = 0; i < capacity; ++i) slots[i].sequence.store(i);
    }
    bool push(const std::uint8_t* bytes, int size, double time) noexcept
    {
        if (!bytes || size <= 0 || size > 3) return false;
        auto position = write.load(std::memory_order_relaxed);
        Slot* slot;
        for (;;)
        {
            slot = &slots[position % capacity];
            const auto sequence = slot->sequence.load(std::memory_order_acquire);
            const auto difference = static_cast<std::intptr_t>(sequence) - static_cast<std::intptr_t>(position);
            if (difference == 0)
            {
                if (write.compare_exchange_weak(position, position + 1, std::memory_order_relaxed)) break;
            }
            else if (difference < 0)
            {
                dropped.fetch_add(1, std::memory_order_relaxed);
                return false;
            }
            else position = write.load(std::memory_order_relaxed);
        }
        std::copy_n(bytes, static_cast<std::size_t>(size), slot->event.bytes.begin());
        slot->event.size = static_cast<std::uint8_t>(size);
        slot->event.timestampSeconds = time;
        slot->sequence.store(position + 1, std::memory_order_release);
        return true;
    }
    bool pop(RealtimeMidiEvent& event) noexcept
    {
        auto& slot = slots[read % capacity];
        if (slot.sequence.load(std::memory_order_acquire) != read + 1) return false;
        event = slot.event;
        // A generated technique may have a newer wall-clock timestamp than the
        // next device event. Preserve causal queue order when JUCE sorts by time.
        event.timestampSeconds = std::max(lastTime, event.timestampSeconds);
        lastTime = event.timestampSeconds;
        slot.sequence.store(read + capacity, std::memory_order_release);
        ++read;
        return true;
    }
    std::uint64_t droppedCount() const noexcept { return dropped.load(std::memory_order_relaxed); }
private:
    static_assert(capacity >= 2);
    std::array<Slot, capacity> slots;
    std::atomic<std::size_t> write { 0 };
    std::size_t read = 0;
    double lastTime = 0.0;
    std::atomic<std::uint64_t> dropped { 0 };
};
}
