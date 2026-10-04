#include "AudioEngineCore.h"

#include <array>
#include <cassert>
#include <cmath>

using namespace fengyin::audioengine;

namespace
{
void pushConstant(SharedAudioRegion& region, float left, float right, std::uint32_t frames)
{
    std::array<float, maximumFramesPerBlock> l {};
    std::array<float, maximumFramesPerBlock> r {};
    l.fill(left);
    r.fill(right);
    const float* channels[] { l.data(), r.data() };
    AudioBlockProducer producer(region);
    assert(producer.tryPush(channels, 2, frames, 0));
}
}

int main()
{
    SharedAudioRegion instrumentRegion;
    SharedAudioRegion systemRegion;
    initialiseRegion(instrumentRegion);
    initialiseRegion(systemRegion);
    instrumentRegion.producerActive.store(1);
    systemRegion.producerActive.store(1);
    pushConstant(instrumentRegion, 0.25f, -0.25f, 128);
    pushConstant(systemRegion, 0.5f, 0.25f, 128);

    AudioEngineCore core(instrumentRegion, systemRegion);
    std::array<float, 128> left {};
    std::array<float, 128> right {};
    float* outputs[] { left.data(), right.data() };
    core.render(outputs, 2, 128);
    for (std::size_t i = 0; i < left.size(); ++i)
    {
        assert(std::abs(left[i] - 0.75f) < 0.000001f);
        assert(std::abs(right[i]) < 0.000001f);
    }
    auto counters = core.getCounters();
    assert(counters.renderCallbacks == 1);
    assert(counters.instrumentUnderflows == 0);
    assert(counters.systemUnderflows == 0);

    pushConstant(instrumentRegion, 0.8f, 0.8f, 128);
    pushConstant(systemRegion, 0.8f, 0.4f, 128);
    core.render(outputs, 2, 128);
    for (std::size_t i = 0; i < left.size(); ++i)
    {
        assert(std::abs(left[i] - 1.0f) < 0.000001f);
        assert(std::abs(right[i] - 1.0f) < 0.000001f);
    }
    assert(core.getCounters().clippedFrames == 128);

    core.render(outputs, 2, 128);
    for (const auto value : left) assert(value == 0.0f);
    assert(core.getCounters().instrumentUnderflows == 1);
    assert(core.getCounters().systemUnderflows == 1);

    // An idle producer is quiet, not broken. It must not poison the dropout
    // diagnostics that drive automatic stability decisions.
    instrumentRegion.producerActive.store(0);
    systemRegion.producerActive.store(0);
    core.render(outputs, 2, 128);
    assert(core.getCounters().instrumentUnderflows == 1);
    assert(core.getCounters().systemUnderflows == 1);

    // The system stream may be clocked by the virtual endpoint rather than the
    // physical device. A healthy backlog must be consumed without underflow
    // while the adaptive reader applies its bounded drift correction.
    core.reset();
    initialiseRegion(instrumentRegion);
    initialiseRegion(systemRegion);
    instrumentRegion.producerActive.store(1);
    systemRegion.producerActive.store(1);
    for (int block = 0; block < 12; ++block)
    {
        pushConstant(instrumentRegion, 0.0f, 0.0f, 128);
        pushConstant(systemRegion, 0.2f, -0.2f, 128);
    }
    for (int block = 0; block < 8; ++block)
    {
        core.render(outputs, 2, 128);
        for (std::size_t i = 0; i < left.size(); ++i)
        {
            assert(std::isfinite(left[i]));
            assert(std::isfinite(right[i]));
            assert(std::abs(left[i] - 0.2f) < 0.0001f);
            assert(std::abs(right[i] + 0.2f) < 0.0001f);
        }
    }
    assert(core.getCounters().systemUnderflows == 0);

    EngineLifecycle lifecycle;
    assert(lifecycle.get() == StreamState::stopped);
    assert(lifecycle.beginStart());
    assert(lifecycle.markRunning());
    assert(lifecycle.beginRecovery());
    assert(lifecycle.markRunning());
    assert(lifecycle.useFallback());
    assert(lifecycle.beginStart());
    lifecycle.stop();
    assert(lifecycle.get() == StreamState::stopped);
    return 0;
}
