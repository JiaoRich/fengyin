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
    // Windows 普通音频模式下跟随系统默认输出，使 WebView 视频和软音源始终去往同一耳机/音响。
    // ASIO 有独立的低延迟设备路由，不在这里强制覆盖用户选择。
    bool followSystemDefaultOutput();
    [[nodiscard]] juce::AudioDeviceManager& getDeviceManager() noexcept { return manager; }

private:
    juce::AudioIODeviceType* findType(const juce::String& typeName);
    void saveSettings();

    juce::AudioDeviceManager manager;
    juce::ApplicationProperties properties;
    juce::String lastError;
};
}
