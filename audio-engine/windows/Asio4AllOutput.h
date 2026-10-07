#pragma once

#include "AudioEngineCore.h"
#include <juce_audio_devices/juce_audio_devices.h>
#include <windows.h>
#include <atomic>
#include <memory>
#include <string>

namespace fengyin::audioengine
{
// The only physical backend of the isolated bridge. JUCE owns ASIO buffer
// conversion and the driver lifecycle; no WASAPI device is opened here.
class Asio4AllOutput final : private juce::AudioIODeviceCallback
{
public:
    ~Asio4AllOutput() override { stop(); }
    bool start(AudioEngineCore&, std::uint32_t, const std::wstring&, std::wstring&, bool requireMatch = false);
    void stop() noexcept;
    bool isRunning() const noexcept;
    std::uint32_t actualBufferFrames() const noexcept { return frames.load(); }
    std::uint32_t outputLatencyFrames() const noexcept { return latency.load(); }

private:
    void audioDeviceIOCallbackWithContext(const float* const*, int, float* const*, int,
                                         int, const juce::AudioIODeviceCallbackContext&) override;
    void audioDeviceAboutToStart(juce::AudioIODevice*) override;
    void audioDeviceStopped() override { running.store(false); }
    void audioDeviceError(const juce::String&) override { running.store(false); }
    std::unique_ptr<juce::AudioIODeviceType> type;
    std::unique_ptr<juce::AudioIODevice> device;
    AudioEngineCore* core = nullptr;
    HANDLE request = nullptr;
    std::atomic<bool> running { false };
    std::atomic<std::uint32_t> frames { 0 }, latency { 0 };
    std::atomic<juce::int64> lastCallback { 0 };
};
}
