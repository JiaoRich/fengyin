#pragma once

#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_data_structures/juce_data_structures.h>
#include <atomic>
#include <array>
#include <functional>
#include <memory>
#include <vector>

#include "BreathMapper.h"
#include "DeviceProfile.h"
#include "ControllerDetector.h"
#include "IntelligentTechniqueProcessor.h"
#include "MidiPerformanceSink.h"
#include "PitchKey.h"

namespace fengyin
{
struct MidiSnapshot
{
    bool deviceConnected = false;
    int lastNote = -1;
    float velocity = 0.0f;
    float breath = 0.0f;
    float pitchBend = 0.0f;
    uint64_t messageCount = 0;
};

struct ExpressionSettings
{
    float threshold = 0.02f;
    float curve = 0.9f;
    float smoothing = 0.28f;
    float pitchSensitivity = 1.0f;
};

struct TechniqueLearnResult
{
    bool ready = false;
    TechniqueMapping mapping;
};

class MidiInputService final : private juce::MidiInputCallback
{
public:
    MidiInputService();
    ~MidiInputService() override;

    std::vector<juce::MidiDeviceInfo> getAvailableDevices() const;
    bool connect(const juce::String& identifier);
    void disconnect();
    void refreshAndConnectFirstAvailable();
    void pollConnection();
    [[nodiscard]] MidiSnapshot getSnapshot() const noexcept;
    [[nodiscard]] juce::String getConnectedDeviceName() const;
    [[nodiscard]] DeviceProfile getActiveProfile() const;
    [[nodiscard]] int getBreathController() const noexcept { return breathController.load(std::memory_order_relaxed); }
    [[nodiscard]] ExpressionSettings getExpressionSettings() const;
    void setExpressionSettings(ExpressionSettings settings);
    void setBreathController(int controllerNumber);
    void setPerformanceSink(MidiPerformanceSink* sink) noexcept;
    void beginBreathDetection() noexcept;
    [[nodiscard]] int finishBreathDetection() noexcept;
    // 只有用户主动开始后，才把下一次 C 指法吹奏用于识别；本调不跨连接保存。
    void beginKeyCalibration() noexcept;
    void cancelKeyCalibration() noexcept;
    void setTargetKey(int pitchClass);
    void setTransposeSemitones(int semitones); // 兼容旧设置：视为以 C 调为来源的目标偏移。
    [[nodiscard]] int getSourceKey() const noexcept { return sourceKey.load(std::memory_order_relaxed); }
    [[nodiscard]] int getTargetKey() const noexcept { return targetKey.load(std::memory_order_relaxed); }
    [[nodiscard]] bool isKeyCalibrationPending() const noexcept { return keyCalibrationPending.load(std::memory_order_acquire); }
    [[nodiscard]] bool isKeyCalibrated() const noexcept { return keyCalibrated.load(std::memory_order_acquire); }
    [[nodiscard]] int getTransposeSemitones() const noexcept { return transposeSemitones.load(std::memory_order_relaxed); }
    void setTechniqueMappings(const juce::Array<TechniqueMapping>& mappings);
    [[nodiscard]] juce::Array<TechniqueMapping> getTechniqueMappings() const;
    void setTechniqueContext(const juce::String& instrumentFamily);
    void beginTechniqueLearn(PerformanceTechnique technique) noexcept;
    void cancelTechniqueLearn() noexcept;
    [[nodiscard]] TechniqueLearnResult consumeTechniqueLearnResult() noexcept;

private:
    void handleIncomingMidiMessage(juce::MidiInput*, const juce::MidiMessage&) override;
    juce::String controllerSettingKey() const;
    juce::String techniqueSettingKey() const;
    void savePreferences();
    void saveTechniqueMappings();
    void loadTechniqueMappings();
    bool handleTechniqueMessage(const juce::MidiMessage& message, MidiPerformanceSink* sink);
    void updateBreathDrivenTechniques(float mappedBreath, MidiPerformanceSink* sink, double nowMs);
    static float techniqueMessageValue(const juce::MidiMessage& message) noexcept;
    void updateEffectiveTranspose();

    std::unique_ptr<juce::MidiInput> input;
    juce::String connectedName;
    juce::String connectedIdentifier;
    juce::ApplicationProperties properties;
    BreathMapper breathMapper;
    mutable juce::SpinLock breathMapperLock;
    DeviceProfile activeProfile = DeviceProfileMatcher::match("");
    ControllerDetector controllerDetector;
    juce::SpinLock detectorLock;
    std::atomic<bool> detectingBreath { false };
    std::atomic<MidiPerformanceSink*> performanceSink { nullptr };
    std::atomic<int> breathController { 2 };
    std::atomic<int> lastNote { -1 };
    std::atomic<float> velocity { 0.0f };
    std::atomic<float> breath { 0.0f };
    std::atomic<float> pitchBend { 0.0f };
    std::atomic<float> pitchSensitivity { 1.0f };
    std::atomic<uint64_t> messageCount { 0 };
    std::atomic<int> transposeSemitones { 0 };
    std::atomic<int> sourceKey { 0 };
    std::atomic<int> targetKey { 0 };
    std::atomic<bool> keyCalibrationPending { false };
    std::atomic<bool> keyCalibrated { false };
    mutable juce::SpinLock noteMapLock;
    std::array<int, 128> activeOutputNotes {};
    mutable juce::SpinLock techniqueLock;
    juce::Array<TechniqueMapping> techniqueMappings;
    juce::String techniqueContext { "other" };
    std::array<float, static_cast<size_t>(PerformanceTechnique::count)> techniquePreviousInput {};
    std::array<bool, static_cast<size_t>(PerformanceTechnique::count)> techniqueToggleState {};
    std::array<float, static_cast<size_t>(PerformanceTechnique::count)> techniqueHardwareInput {};
    std::array<float, static_cast<size_t>(PerformanceTechnique::count)> techniqueBreathInput {};
    IntelligentTechniqueProcessor intelligentTechniques;
    int activeNoteCount = 0;
    std::atomic<bool> learningTechnique { false };
    std::atomic<int> learningTechniqueId { 0 };
    std::atomic<int> learnedSourceType { 0 };
    std::atomic<int> learnedSourceNumber { -1 };
    std::atomic<bool> learnedSourceReady { false };
};
}
