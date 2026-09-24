#pragma once

#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_data_structures/juce_data_structures.h>
#include <optional>
#include <vector>

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
    // 检测耳机、音响或 USB 声卡是否已成为 Windows 默认输出。
    // 手动模式尊重专业用户选择；自动模式才跟随系统。
    [[nodiscard]] bool systemDefaultOutputChanged();
    // Windows 共享模式下跟随系统默认输出，使伴奏和软音源去往同一耳机/音响。
    bool followSystemDefaultOutput();
    // 对当前输出设备应用适合实时演奏的设置：关闭输入、优先 48 kHz，并选用设备可稳定支持的低延迟缓冲。
    juce::String optimiseForLivePerformance();
    // 只监测持续丢音，不在演奏中偷偷重启设备或放大缓冲。
    bool hasSustainedRuntimeInstability();
    // 首次启动或检测到新输出设备时，在后台依次试跑可用的低延迟方案。
    // 默认只在自动模式下执行；force 仅用于用户主动点击“恢复自动优化”。
    bool beginAutomaticLatencyTuning(bool force = false);
    [[nodiscard]] bool needsAutomaticLatencyTuning();
    [[nodiscard]] bool isAutomaticMode();
    [[nodiscard]] bool isAutomaticLatencyTuning() const noexcept { return tuningActive; }
    [[nodiscard]] double getTuningProgress() const noexcept;
    void cancelAutomaticLatencyTuning();
    void updateProbeEvidence(uint64_t callbacks, uint64_t overruns, uint64_t signals) noexcept
    { probeCallbacks = callbacks; probeOverruns = overruns; probeSignals = signals; }
    // 由界面定时器非阻塞轮询；仅在调优结束时返回用户可读的结果。
    std::optional<juce::String> pollAutomaticLatencyTuning();
    [[nodiscard]] juce::AudioDeviceManager& getDeviceManager() noexcept { return manager; }

private:
    juce::AudioIODeviceType* findType(const juce::String& typeName);
    juce::String preferredLiveDeviceType();
    juce::String configureAutomaticType(const juce::String& typeName);
    juce::String currentDeviceSignature() const;
    juce::String tuningIdentity();
    struct LatencyCandidate
    {
        juce::String typeName;
        juce::String outputName;
        int preferredBuffer = 128;
        int priority = 0;
    };
    struct LatencyResult
    {
        LatencyCandidate candidate;
        AudioDeviceStatus status;
        double maximumCpu = 0.0;
        int addedXRuns = 0;
        bool stable = false;
        double score = 1.0e9;
    };
    void buildLatencyCandidates();
    bool applyLatencyCandidate(const LatencyCandidate& candidate);
    bool startNextLatencyCandidate();
    juce::String finishAutomaticLatencyTuning();
    void saveSettings();

    juce::AudioDeviceManager manager;
    juce::ApplicationProperties properties;
    juce::String lastError;
    int observedXRunCount = 0;
    int unstablePolls = 0;
    bool tuningActive = false;
    juce::String attemptedTuningIdentity;
    juce::String cachedHardwareIdentity;
    double hardwareIdentityCheckedAt = -100000.0;
    int tuningCandidateIndex = -1;
    double tuningCandidateStartedAtMs = 0.0;
    int tuningCandidateStartXRuns = 0;
    double tuningCandidateMaximumCpu = 0.0;
    uint64_t probeCallbacks = 0, probeOverruns = 0, probeSignals = 0;
    uint64_t startCallbacks = 0, startOverruns = 0, startSignals = 0;
    juce::String tuningFallbackType;
    juce::String tuningFallbackOutput;
    double tuningFallbackRate = 0.0;
    int tuningFallbackBuffer = 0;
    std::vector<LatencyCandidate> tuningCandidates;
    std::vector<LatencyResult> tuningResults;
};
}
