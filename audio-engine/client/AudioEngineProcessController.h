#pragma once

#include "NamedSharedAudioRegion.h"

#include <juce_core/juce_core.h>

#include <cstdint>

namespace fengyin::audioengine
{
class AudioEngineProcessController
{
public:
    AudioEngineProcessController() = default;
    ~AudioEngineProcessController();
    AudioEngineProcessController(const AudioEngineProcessController&) = delete;
    AudioEngineProcessController& operator=(const AudioEngineProcessController&) = delete;

    bool start(std::uint32_t bufferFrames, const juce::String& physicalEndpointId,
               bool routeSystemAudio, juce::String& error);
    [[nodiscard]] static bool isAvailable() noexcept;
    void stop() noexcept;
    [[nodiscard]] bool isRunning() const noexcept;
    [[nodiscard]] std::uint32_t actualBufferFrames() const noexcept;
    bool requestTestTone() noexcept
    {
        if (! isRunning() || statusMapping.get() == nullptr) return false;
        statusMapping.get()->testToneRequest.store(1);
        return true;
    }

private:
    bool waitUntilRunning(int timeoutMilliseconds, juce::String& error) noexcept;

    NamedSharedAudioRegion statusMapping;
#if defined(_WIN32)
    PROCESS_INFORMATION process {};
#endif
    bool ownsProcess = false;
};
}
