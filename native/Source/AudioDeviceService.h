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
    [[nodiscard]] juce::Array<double> getAvailableSampleRates();
    [[nodiscard]] juce::Array<int> getAvailableBufferSizes();
    juce::String selectDeviceType(const juce::String& typeName);
    juce::String applyOutputSetup(const juce::String& outputName, double sampleRate, int bufferSize);
    // 首次运行或自动跟随到新设备时，优先使用 Windows 低延迟共享模式、48 kHz 和 128 采样。
    // 用户在设置面板手动应用过配置后，不再自动覆盖。
    juce::String applyBestInitialSetup();
    // Windows 普通音频模式下跟随系统默认输出，使 WebView 视频和软音源始终去往同一耳机/音响。
    // ASIO 有独立的低延迟设备路由，不在这里强制覆盖用户选择。
    bool followSystemDefaultOutput();
    // 对当前输出设备应用适合实时演奏的设置：关闭输入、优先 48 kHz，并选用设备可稳定支持的低延迟缓冲。
    juce::String optimiseForLivePerformance();
    // 出现连续丢音时只上调一级缓冲，避免反复爆音；返回 true 表示已自动降级。
    bool stabiliseAfterXRuns();
    [[nodiscard]] juce::AudioDeviceManager& getDeviceManager() noexcept { return manager; }

private:
    juce::AudioIODeviceType* findType(const juce::String& typeName);
    juce::String configureAutomaticType(const juce::String& typeName);
    juce::String currentDeviceSignature() const;
    void saveSettings();

    juce::AudioDeviceManager manager;
    juce::ApplicationProperties properties;
    juce::String lastError;
    int observedXRunCount = 0;
    int unstablePolls = 0;
};
}
