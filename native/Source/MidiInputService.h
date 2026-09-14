#pragma once

#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_data_structures/juce_data_structures.h>
#include <atomic>
#include <functional>
#include <memory>
#include <vector>

#include "BreathMapper.h"
#include "DeviceProfile.h"
#include "ControllerDetector.h"
#include "MidiPerformanceSink.h"

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

private:
    void handleIncomingMidiMessage(juce::MidiInput*, const juce::MidiMessage&) override;
    juce::String controllerSettingKey() const;
    void savePreferences();

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
};
}
