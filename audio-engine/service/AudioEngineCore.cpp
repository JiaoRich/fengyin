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
        const auto peak = std::max(std::abs(left), std::abs(right));
        if (peak > 1.0f)
        {
            const auto safety = 1.0f / peak;
            left *= safety;
            right *= safety;
            ++clipped;
        }
        if (outputs[0] != nullptr)
            outputs[0][frame] = left;
        if (outputChannels > 1 && outputs[1] != nullptr)
            outputs[1][frame] = right;
        for (std::uint32_t channel = 2; channel < outputChannels; ++channel)
            if (outputs[channel] != nullptr) outputs[channel][frame] = 0.0f;
    }
    if (instrumentMissing) instrumentUnderflows.fetch_add(1, std::memory_order_relaxed);
    if (systemMissing) systemUnderflows.fetch_add(1, std::memory_order_relaxed);
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

