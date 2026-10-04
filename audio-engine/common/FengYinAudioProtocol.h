#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <type_traits>

namespace fengyin::audioengine
{
constexpr std::uint32_t protocolMagic = 0x45415946u; // "FYAE" in little endian
constexpr std::uint16_t protocolMajor = 2;
constexpr std::uint16_t protocolMinor = 0;
constexpr std::uint32_t engineSampleRate = 48000;
constexpr std::uint16_t engineChannels = 2;
constexpr std::uint16_t maximumFramesPerBlock = 512;
constexpr std::size_t audioBlockSlots = 32;

enum class StreamState : std::uint32_t
{
    stopped = 0,
    starting,
    running,
    recovering,
    fallback
};

enum AudioBlockFlags : std::uint32_t
{
    blockNone = 0,
    blockDiscontinuity = 1u << 0,
    blockSilence = 1u << 1,
    blockEndOfStream = 1u << 2
};

struct alignas(64) ProtocolHeader
{
    std::uint32_t magic = protocolMagic;
    std::uint16_t major = protocolMajor;
    std::uint16_t minor = protocolMinor;
    std::uint32_t headerBytes = sizeof(ProtocolHeader);
    std::uint32_t sampleRate = engineSampleRate;
    std::uint16_t channels = engineChannels;
    std::uint16_t maxFramesPerBlock = maximumFramesPerBlock;
    std::uint32_t slotCount = static_cast<std::uint32_t>(audioBlockSlots);
    std::uint32_t reserved = 0;
};

struct alignas(64) AudioBlock
{
    std::uint64_t sequence = 0;
    std::uint64_t qpcTimestamp = 0;
    std::uint32_t frameCount = 0;
    std::uint32_t flags = blockNone;
    std::array<float, static_cast<std::size_t>(maximumFramesPerBlock) * engineChannels> samples {};
};

struct alignas(64) SharedAudioRegion
{
    ProtocolHeader protocol;
    std::atomic<std::uint64_t> writeSequence { 0 };
    std::atomic<std::uint64_t> readSequence { 0 };
    std::atomic<std::uint64_t> producerHeartbeat { 0 };
    std::atomic<std::uint64_t> consumerHeartbeat { 0 };
    std::atomic<std::uint64_t> droppedBlocks { 0 };
    std::atomic<std::uint32_t> state { static_cast<std::uint32_t>(StreamState::stopped) };
    // The physical backend publishes the period it actually obtained. Some
    // drivers align a requested 128/256-frame period to a nearby legal value;
    // the producer must render that real size or its queue slowly accumulates
    // latency even though neither side reports an xrun.
    std::atomic<std::uint32_t> activePeriodFrames { 0 };
    std::array<AudioBlock, audioBlockSlots> blocks {};
};

static_assert(std::atomic<std::uint64_t>::is_always_lock_free,
              "The shared audio protocol requires lock-free 64-bit atomics");
static_assert(std::is_standard_layout_v<ProtocolHeader>);
static_assert(std::is_standard_layout_v<AudioBlock>);

inline bool isCompatible(const ProtocolHeader& header) noexcept
{
    return header.magic == protocolMagic
        && header.major == protocolMajor
        && header.headerBytes == sizeof(ProtocolHeader)
        && header.sampleRate == engineSampleRate
        && header.channels == engineChannels
        && header.maxFramesPerBlock == maximumFramesPerBlock
        && header.slotCount == audioBlockSlots;
}

inline void initialiseRegion(SharedAudioRegion& region) noexcept
{
    region.protocol = {};
    region.writeSequence.store(0, std::memory_order_relaxed);
    region.readSequence.store(0, std::memory_order_relaxed);
    region.producerHeartbeat.store(0, std::memory_order_relaxed);
    region.consumerHeartbeat.store(0, std::memory_order_relaxed);
    region.droppedBlocks.store(0, std::memory_order_relaxed);
    region.state.store(static_cast<std::uint32_t>(StreamState::stopped), std::memory_order_relaxed);
    region.activePeriodFrames.store(0, std::memory_order_relaxed);
    for (auto& block : region.blocks)
        block = {};
}

class AudioBlockProducer
{
public:
    explicit AudioBlockProducer(SharedAudioRegion& storage) noexcept : region(storage) {}

    bool tryPush(const float* const* channels,
                 std::uint32_t channelCount,
                 std::uint32_t frames,
                 std::uint64_t qpcTimestamp,
                 std::uint32_t flags = blockNone) noexcept
    {
        if (! isCompatible(region.protocol) || frames > maximumFramesPerBlock)
            return false;

        const auto write = region.writeSequence.load(std::memory_order_relaxed);
        const auto read = region.readSequence.load(std::memory_order_acquire);
        if (write - read >= audioBlockSlots)
        {
            region.droppedBlocks.fetch_add(1, std::memory_order_relaxed);
            return false;
        }

        auto& block = region.blocks[static_cast<std::size_t>(write % audioBlockSlots)];
        block.sequence = write;
        block.qpcTimestamp = qpcTimestamp;
        block.frameCount = frames;
        block.flags = flags;
        const auto copyChannels = channelCount > engineChannels ? engineChannels : channelCount;
        for (std::uint32_t frame = 0; frame < frames; ++frame)
            for (std::uint32_t channel = 0; channel < engineChannels; ++channel)
                block.samples[static_cast<std::size_t>(frame) * engineChannels + channel]
                    = channel < copyChannels && channels != nullptr && channels[channel] != nullptr
                    ? channels[channel][frame] : 0.0f;

        region.writeSequence.store(write + 1, std::memory_order_release);
        region.producerHeartbeat.fetch_add(1, std::memory_order_relaxed);
        return true;
    }

private:
    SharedAudioRegion& region;
};

class AudioBlockConsumer
{
public:
    explicit AudioBlockConsumer(SharedAudioRegion& storage) noexcept : region(storage) {}

    bool tryPop(AudioBlock& destination) noexcept
    {
        if (! isCompatible(region.protocol))
            return false;
        const auto read = region.readSequence.load(std::memory_order_relaxed);
        const auto write = region.writeSequence.load(std::memory_order_acquire);
        if (read == write)
            return false;
        destination = region.blocks[static_cast<std::size_t>(read % audioBlockSlots)];
        region.readSequence.store(read + 1, std::memory_order_release);
        region.consumerHeartbeat.fetch_add(1, std::memory_order_relaxed);
        return true;
    }

    [[nodiscard]] std::uint64_t queuedBlocks() const noexcept
    {
        const auto read = region.readSequence.load(std::memory_order_relaxed);
        const auto write = region.writeSequence.load(std::memory_order_acquire);
        return write - read;
    }

private:
    SharedAudioRegion& region;
};
}
