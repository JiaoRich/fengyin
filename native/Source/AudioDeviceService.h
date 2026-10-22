#pragma once

#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_data_structures/juce_data_structures.h>
#include <optional>
#include <vector>

#include "AudioEngineProcessController.h"

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

class AudioDeviceService final : private juce::Timer
{
public:
    AudioDeviceService();
    ~AudioDeviceService() override;

    juce::String initialise();
    [[nodiscard]] AudioDeviceStatus getStatus();
    [[nodiscard]] juce::StringArray getAvailableDeviceTypes();
    [[nodiscard]] juce::StringArray getAvailableOutputDevices(const juce::String& typeName);
    [[nodiscard]] juce::Array<double> getAvailableSampleRates();
    [[nodiscard]] juce::Array<int> getAvailableBufferSizes();
    [[nodiscard]] bool isSharedAsioModeAvailable();
    [[nodiscard]] bool isSharedAsioModeActive();
    juce::String selectDeviceType(const juce::String& typeName);
    juce::String configureAsio4All();
    juce::String requestEngineTestTone() { return engineProcess.requestTestTone()
        ? juce::String::fromUTF8("正在播放2秒低音量测试音，请记录是否听到。")
        : juce::String::fromUTF8("独立引擎未运行，无法播放测试音。请直接导出诊断，不必反复切换。"); }
    juce::String applyOutputSetup(const juce::String& outputName, double sampleRate, int bufferSize);
    // 首次运行或自动跟随到新设备时，优先使用 Windows 低延迟共享模式、48 kHz 和 128 采样。
    // 用户在设置面板手动应用过配置后，不再自动覆盖。
    juce::String applyBestInitialSetup();
    // 检测耳机、音响或 USB 声卡是否已成为 Windows 默认输出。
    // 手动模式尊重专业用户选择；自动模式才跟随系统。
    [[nodiscard]] bool systemDefaultOutputChanged();
    // Windows 共享模式下跟随系统默认输出，使伴奏和软音源去往同一耳机/音响。
    bool followSystemDefaultOutput();
    [[nodiscard]] static bool bridgeDependenciesAvailable();
    // 只监测持续丢音，不在演奏中偷偷重启设备或放大缓冲。
    bool hasSustainedRuntimeInstability();
    [[nodiscard]] bool isAutomaticMode();
    [[nodiscard]] juce::AudioDeviceManager& getDeviceManager() noexcept { return manager; }

private:
    juce::String restorePhysicalSharedOutput();
    juce::String startIsolatedAudioEngine(int requestedFrames = 128);
    juce::AudioIODeviceType* findType(const juce::String& typeName);
    juce::String preferredLiveDeviceType();
    juce::String configureAutomaticType(const juce::String& typeName);
    juce::String currentDeviceSignature() const;
    void saveSettings();
    void timerCallback() override;
    void restoreAfterAsioPanel();
    juce::ChildProcess asioPanelProcess;
    bool asioPanelOpen = false;
    bool resumeIsolatedEngine = false;
    int resumeEngineBuffer = 128;
    double engineRecoveryStartedAt = 0.0;
    std::unique_ptr<juce::XmlElement> beforeAsioPanel;

    juce::AudioDeviceManager manager;
    audioengine::AudioEngineProcessController engineProcess;
    bool engineDeviceTypeAdded = false;
    juce::ApplicationProperties properties;
    juce::String lastError;
    int observedXRunCount = 0;
    int unstablePolls = 0;

};
}
