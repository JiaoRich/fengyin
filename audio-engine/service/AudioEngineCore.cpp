#include "AudioEngineCore.h"

#include <algorithm>
#include <cmath>

namespace fengyin::audioengine
{
AudioEngineCore::AudioEngineCore(SharedAudioRegion& instrumentRegion,
                                 SharedAudioRegion& systemRegion) noexcept
    : instrument(instrumentRegion), system(systemRegion)
{
}

void AudioEngineCore::reset() noexcept
{
    instrument.reset();
    system.reset();
    renderCallbacks.store(0, std::memory_order_relaxed);
    instrumentUnderflows.store(0, std::memory_order_relaxed);
    systemUnderflows.store(0, std::memory_order_relaxed);
    clippedFrames.store(0, std::memory_order_relaxed);
}

bool AudioEngineCore::StreamReader::next(float& left, float& right) noexcept
{
    if (! hasBlock || cursor >= current.frameCount)
    {
        if (! consumer.tryPop(current))
        {
            left = right = 0.0f;
            hasBlock = false;
            return false;
        }
        cursor = 0;
        hasBlock = current.frameCount != 0;
    }

    if (! hasBlock)
    {
        left = right = 0.0f;
        return false;
    }

    const auto offset = static_cast<std::size_t>(cursor) * engineChannels;
    left = current.samples[offset];
    right = current.samples[offset + 1];
    ++cursor;
    return true;
}

void AudioEngineCore::AdaptiveSystemReader::reset() noexcept
{
    cursor = 0;
    current = {};
    hasBlock = false;
    primed = false;
    previousLeft = previousRight = followingLeft = followingRight = 0.0f;
    phase = 0.0;
}

bool AudioEngineCore::AdaptiveSystemReader::nextSource(float& left, float& right) noexcept
{
    if (! hasBlock || cursor >= current.frameCount)
    {
        if (! consumer.tryPop(current))
        {
            hasBlock = false;
            left = right = 0.0f;
            return false;
        }
        cursor = 0;
        hasBlock = current.frameCount != 0;
    }
    if (! hasBlock)
    {
        left = right = 0.0f;
        return false;
    }
    const auto offset = static_cast<std::size_t>(cursor) * engineChannels;
    left = current.samples[offset];
    right = current.samples[offset + 1];
    ++cursor;
    return true;
}

bool AudioEngineCore::AdaptiveSystemReader::next(float& left, float& right) noexcept
{
    if (! primed)
    {
        if (! nextSource(previousLeft, previousRight)
            || ! nextSource(followingLeft, followingRight))
        {
            left = right = 0.0f;
            return false;
        }
        primed = true;
        phase = 0.0;
    }

    left = previousLeft + static_cast<float>((followingLeft - previousLeft) * phase);
    right = previousRight + static_cast<float>((followingRight - previousRight) * phase);
    const auto queued = consumer.queuedBlocks();
    // ±500 ppm comfortably covers independent consumer audio clocks while
    // remaining inaudible for accompaniment and browser playback.
    const auto ratio = queued > 8 ? 1.0005 : queued < 2 ? 0.9995 : 1.0;
    phase += ratio;
    while (phase >= 1.0)
    {
        previousLeft = followingLeft;
        previousRight = followingRight;
        if (! nextSource(followingLeft, followingRight))
        {
            followingLeft = previousLeft;
            followingRight = previousRight;
            primed = false;
            phase = 0.0;
            break;
        }
        phase -= 1.0;
    }
    return true;
}

void AudioEngineCore::render(float* const* outputs, std::uint32_t outputChannels,
                             std::uint32_t frames) noexcept
{
    if (outputs == nullptr || outputChannels == 0 || frames == 0)
        return;
    renderCallbacks.fetch_add(1, std::memory_order_relaxed);

    bool instrumentMissing = false;
    bool systemMissing = false;
    std::uint64_t clipped = 0;
    for (std::uint32_t frame = 0; frame < frames; ++frame)
    {
        float instrumentLeft = 0.0f, instrumentRight = 0.0f;
        float systemLeft = 0.0f, systemRight = 0.0f;
        instrumentMissing = ! instrument.next(instrumentLeft, instrumentRight) || instrumentMissing;
        systemMissing = ! system.next(systemLeft, systemRight) || systemMissing;

        auto left = instrumentLeft + systemLeft;
        auto right = instrumentRight + systemRight;
        if (std::abs(left) > 1.0f || std::abs(right) > 1.0f)
        {
            // Preserve the original system and instrument levels throughout
            // their valid range. Only impossible output values are clamped;
            // there is no hidden compressor or accompaniment ducking here.
            left = std::clamp(left, -1.0f, 1.0f);
            right = std::clamp(right, -1.0f, 1.0f);
            ++clipped;
        }
        if (outputs[0] != nullptr)
            outputs[0][frame] = left;
        if (outputChannels > 1 && outputs[1] != nullptr)
            outputs[1][frame] = right;
        for (std::uint32_t channel = 2; channel < outputChannels; ++channel)
            if (outputs[channel] != nullptr) outputs[channel][frame] = 0.0f;
    }
    if (instrumentMissing && instrument.producerIsActive())
        instrumentUnderflows.fetch_add(1, std::memory_order_relaxed);
    if (systemMissing && system.producerIsActive())
        systemUnderflows.fetch_add(1, std::memory_order_relaxed);
    if (clipped != 0) clippedFrames.fetch_add(clipped, std::memory_order_relaxed);
}

EngineCounters AudioEngineCore::getCounters() const noexcept
{
    return { renderCallbacks.load(std::memory_order_relaxed),
             instrumentUnderflows.load(std::memory_order_relaxed),
             systemUnderflows.load(std::memory_order_relaxed),
             clippedFrames.load(std::memory_order_relaxed) };
}
}
