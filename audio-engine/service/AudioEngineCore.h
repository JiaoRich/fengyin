#pragma once

#include "FengYinAudioProtocol.h"

#include <array>
#include <atomic>
#include <cstdint>

namespace fengyin::audioengine
{
struct EngineCounters
{
    std::uint64_t renderCallbacks = 0;
    std::uint64_t instrumentUnderflows = 0;
    std::uint64_t systemUnderflows = 0;
    std::uint64_t clippedFrames = 0;
};

class AudioEngineCore
{
public:
    AudioEngineCore(SharedAudioRegion& instrumentRegion,
                    SharedAudioRegion& systemRegion) noexcept;

    void reset() noexcept;
    void render(float* const* outputs, std::uint32_t outputChannels,
                std::uint32_t frames) noexcept;
    [[nodiscard]] EngineCounters getCounters() const noexcept;

private:
    class StreamReader
    {
    public:
        explicit StreamReader(SharedAudioRegion& storage) noexcept : consumer(storage) {}
        void reset() noexcept { cursor = 0; current = {}; hasBlock = false; }
        bool next(float& left, float& right) noexcept;
        [[nodiscard]] bool producerIsActive() const noexcept { return consumer.producerIsActive(); }
    private:
        AudioBlockConsumer consumer;
        AudioBlock current;
        std::uint32_t cursor = 0;
        bool hasBlock = false;
    };

    // Windows' virtual render clock and the physical device clock are not
    // crystal-locked. This tiny asynchronous linear resampler prevents the
    // browser stream from slowly filling or draining its ring over a long
    // performance. It never touches the instrument fast path.
    class AdaptiveSystemReader
    {
    public:
        explicit AdaptiveSystemReader(SharedAudioRegion& storage) noexcept : consumer(storage) {}
        void reset() noexcept;
        bool next(float& left, float& right) noexcept;
        [[nodiscard]] bool producerIsActive() const noexcept { return consumer.producerIsActive(); }
    private:
        bool nextSource(float& left, float& right) noexcept;
        AudioBlockConsumer consumer;
        AudioBlock current;
        std::uint32_t cursor = 0;
        bool hasBlock = false;
        bool primed = false;
        float previousLeft = 0.0f, previousRight = 0.0f;
        float followingLeft = 0.0f, followingRight = 0.0f;
        double phase = 0.0;
    };

    StreamReader instrument;
    AdaptiveSystemReader system;
    std::atomic<std::uint64_t> renderCallbacks { 0 };
    std::atomic<std::uint64_t> instrumentUnderflows { 0 };
    std::atomic<std::uint64_t> systemUnderflows { 0 };
    std::atomic<std::uint64_t> clippedFrames { 0 };
};

class EngineLifecycle
{
public:
    [[nodiscard]] StreamState get() const noexcept
    {
        return static_cast<StreamState>(state.load(std::memory_order_acquire));
    }

    bool beginStart() noexcept { return transition(StreamState::stopped, StreamState::starting)
                                    || transition(StreamState::fallback, StreamState::starting); }
    bool markRunning() noexcept { return transition(StreamState::starting, StreamState::running)
                                      || transition(StreamState::recovering, StreamState::running); }
    bool beginRecovery() noexcept { return transition(StreamState::running, StreamState::recovering); }
    bool useFallback() noexcept
    {
        auto current = get();
        while (current != StreamState::fallback && current != StreamState::stopped)
        {
            auto expected = static_cast<std::uint32_t>(current);
            if (state.compare_exchange_weak(expected, static_cast<std::uint32_t>(StreamState::fallback),
                                            std::memory_order_acq_rel))
                return true;
            current = static_cast<StreamState>(expected);
        }
        return current == StreamState::fallback;
    }
    void stop() noexcept { state.store(static_cast<std::uint32_t>(StreamState::stopped), std::memory_order_release); }

private:
    bool transition(StreamState from, StreamState to) noexcept
    {
        auto expected = static_cast<std::uint32_t>(from);
        return state.compare_exchange_strong(expected, static_cast<std::uint32_t>(to),
                                             std::memory_order_acq_rel);
    }
    std::atomic<std::uint32_t> state { static_cast<std::uint32_t>(StreamState::stopped) };
};
}
