#pragma once

#include <juce_audio_devices/juce_audio_devices.h>
#include <array>
#include <atomic>

#include "MidiPerformanceSink.h"

namespace fengyin
{
class RecordingService;
class AccompanimentAudioService;
class MasterOutputService;
class TestSynthEngine final : public juce::AudioIODeviceCallback,
                              public MidiPerformanceSink
{
public:
    void setRecordingService(RecordingService* service) noexcept { recorder = service; }
    void setAccompanimentService(AccompanimentAudioService* service) noexcept { accompaniment = service; }
    void setMasterOutputService(MasterOutputService* service) noexcept { masterOutput = service; }
    void noteOn(int noteNumber, float velocity) noexcept override;
    void noteOff(int noteNumber) noexcept override;
    void breathChanged(float value) noexcept override;
    void pitchBendChanged(float bipolarValue) noexcept override;

    void audioDeviceIOCallbackWithContext(const float* const* inputChannelData,
                                          int numInputChannels,
                                          float* const* outputChannelData,
                                          int numOutputChannels,
                                          int numSamples,
                                          const juce::AudioIODeviceCallbackContext&) override;
    void audioDeviceAboutToStart(juce::AudioIODevice*) override;
    void audioDeviceStopped() override;

    [[nodiscard]] float getLeftPeak() const noexcept { return leftPeak.load(std::memory_order_relaxed); }
    [[nodiscard]] float getRightPeak() const noexcept { return rightPeak.load(std::memory_order_relaxed); }

private:
    enum class CommandType { noteOn, noteOff, breath, pitchBend };
    struct Command { CommandType type; int note = 0; float value = 0.0f; };

    static constexpr std::size_t queueSize = 256;
    bool push(Command command) noexcept;
    bool pop(Command& command) noexcept;
    void applyPendingCommands() noexcept;
    void updateFrequency() noexcept;

    std::array<Command, queueSize> queue {};
    std::atomic<std::size_t> writeIndex { 0 };
    std::atomic<std::size_t> readIndex { 0 };
    std::atomic<float> leftPeak { 0.0f };
    std::atomic<float> rightPeak { 0.0f };

    double sampleRate = 48000.0;
    double phase = 0.0;
    double phaseDelta = 0.0;
    int activeNote = -1;
    float noteVelocity = 0.0f;
    float targetBreath = 0.0f;
    float smoothedBreath = 0.0f;
    float pitchBend = 0.0f;
    RecordingService* recorder = nullptr;
    AccompanimentAudioService* accompaniment = nullptr;
    MasterOutputService* masterOutput = nullptr;
};
}
