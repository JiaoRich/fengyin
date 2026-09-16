#pragma once

#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_data_structures/juce_data_structures.h>

namespace fengyin
{
struct AudioDeviceStatus
{
    bool ready = false;
    juce::String deviceType;
    juce::String deviceName;
    double sampleRate = 0.0;
    int bufferSize = 0;
    double estimatedBufferLatencyMs = 0.0;
    double cpuUsage = 0.0;
    int xRunCount = 0;
    juce::String error;
};

class AudioDeviceService final
{
public:
    AudioDeviceService();
    ~AudioDeviceService() = default;

    juce::String initialise();
    [[nodiscard]] AudioDeviceStatus getStatus();
    [[nodiscard]] juce::StringArray getAvailableDeviceTypes();
    [[nodiscard]] juce::StringArray getAvailableOutputDevices(const juce::String& typeName);
    juce::String selectDeviceType(const juce::String& typeName);
    juce::String applyOutputSetup(const juce::String& outputName, double sampleRate, int bufferSize);
    [[nodiscard]] juce::AudioDeviceManager& getDeviceManager() noexcept { return manager; }

private:
    juce::AudioIODeviceType* findType(const juce::String& typeName);
    void saveSettings();

    juce::AudioDeviceManager manager;
    juce::ApplicationProperties properties;
    juce::String lastError;
};
}
