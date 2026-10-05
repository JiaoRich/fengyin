#include "FengYinAudioProtocol.h"

#include <array>
#include <cassert>
#include <cmath>

using namespace fengyin::audioengine;

int main()
{
    SharedAudioRegion region;
    initialiseRegion(region);
    assert(isCompatible(region.protocol));
    assert(region.activePeriodFrames.load() == 0);
    assert(region.physicalOutputLatencyFrames.load() == 0);
    assert(region.producerActive.load() == 0);

    AudioBlockProducer producer(region);
    AudioBlockConsumer consumer(region);
    std::array<float, 128> left {};
    std::array<float, 128> right {};
    for (std::size_t i = 0; i < left.size(); ++i)
    {
        left[i] = static_cast<float>(i) / 128.0f;
        right[i] = -left[i];
    }
    const float* channels[] { left.data(), right.data() };
    assert(producer.tryPush(channels, 2, 128, 123456));
    assert(region.producerHeartbeat.load() == 1);

    AudioBlock block;
    assert(consumer.tryPop(block));
    assert(region.consumerHeartbeat.load() == 1);
    assert(block.sequence == 0);
    assert(block.qpcTimestamp == 123456);
    assert(block.frameCount == 128);
    for (std::size_t i = 0; i < left.size(); ++i)
    {
        assert(std::abs(block.samples[i * 2] - left[i]) < 0.000001f);
        assert(std::abs(block.samples[i * 2 + 1] - right[i]) < 0.000001f);
    }
    assert(! consumer.tryPop(block));

    const float* mono[] { left.data() };
    assert(producer.tryPush(mono, 1, 128, 222));
    assert(consumer.tryPop(block));
    for (std::size_t i = 0; i < left.size(); ++i)
    {
        assert(std::abs(block.samples[i * 2] - left[i]) < 0.000001f);
        assert(block.samples[i * 2 + 1] == 0.0f);
    }

    for (std::size_t index = 0; index < audioBlockSlots; ++index)
        assert(producer.tryPush(channels, 2, 128, index));
    assert(! producer.tryPush(channels, 2, 128, 999));
    assert(region.droppedBlocks.load() == 1);
    for (std::size_t index = 0; index < audioBlockSlots; ++index)
    {
        assert(consumer.tryPop(block));
        assert(block.qpcTimestamp == index);
    }

    assert(! producer.tryPush(channels, 2, maximumFramesPerBlock + 1, 0));
    region.protocol.major += 1;
    assert(! producer.tryPush(channels, 2, 128, 0));
    assert(! consumer.tryPop(block));
    return 0;
}
