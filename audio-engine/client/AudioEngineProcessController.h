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
               juce::String& error);
    void stop() noexcept;
    [[nodiscard]] bool isRunning() const noexcept;
    [[nodiscard]] std::uint32_t actualBufferFrames() const noexcept;

private:
    bool waitUntilRunning(int timeoutMilliseconds, juce::String& error) noexcept;

    NamedSharedAudioRegion statusMapping;
#if defined(_WIN32)
    PROCESS_INFORMATION process {};
#endif
    bool ownsProcess = false;
};
}
