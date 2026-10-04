#pragma once

#include "FengYinAudioProtocol.h"
#include "NamedSharedAudioRegion.h"

#include <juce_audio_devices/juce_audio_devices.h>

#include <array>
#include <atomic>
#include <memory>
#include <thread>

namespace fengyin::audioengine
{
class FengYinEngineAudioIODevice final : public juce::AudioIODevice
{
public:
    FengYinEngineAudioIODevice();
    ~FengYinEngineAudioIODevice() override;

    juce::StringArray getOutputChannelNames() override;
    juce::StringArray getInputChannelNames() override;
    std::optional<juce::BigInteger> getDefaultOutputChannels() const override;
    std::optional<juce::BigInteger> getDefaultInputChannels() const override;
    juce::Array<double> getAvailableSampleRates() override;
    juce::Array<int> getAvailableBufferSizes() override;
    int getDefaultBufferSize() override;
    juce::String open(const juce::BigInteger& inputChannels,
                      const juce::BigInteger& outputChannels,
                      double sampleRate, int bufferSizeSamples) override;
    void close() override;
    bool isOpen() override;
    void start(juce::AudioIODeviceCallback* callback) override;
    void stop() override;
    bool isPlaying() override;
    juce::String getLastError() override;
    int getCurrentBufferSizeSamples() override;
    double getCurrentSampleRate() override;
    int getCurrentBitDepth() override;
    juce::BigInteger getActiveOutputChannels() const override;
    juce::BigInteger getActiveInputChannels() const override;
    int getOutputLatencyInSamples() override;
    int getInputLatencyInSamples() override;
    int getXRunCount() const noexcept override;

private:
    void run() noexcept;

    NamedSharedAudioRegion mapping;
    std::unique_ptr<AudioBlockProducer> producer;
    juce::AudioIODeviceCallback* activeCallback = nullptr;
    std::thread pump;
    std::atomic<bool> opened { false };
    std::atomic<bool> playing { false };
    std::atomic<bool> stopRequested { false };
    std::atomic<int> xruns { 0 };
    int bufferFrames = 256;
    juce::String error;
#if defined(_WIN32)
    HANDLE requestSemaphore = nullptr;
#endif
};

class FengYinEngineAudioIODeviceType final : public juce::AudioIODeviceType
{
public:
    FengYinEngineAudioIODeviceType();
    void scanForDevices() override {}
    juce::StringArray getDeviceNames(bool wantInputNames) const override;
    int getDefaultDeviceIndex(bool forInput) const override;
    int getIndexOfDevice(juce::AudioIODevice* device, bool asInput) const override;
    bool hasSeparateInputsAndOutputs() const override { return true; }
    juce::AudioIODevice* createDevice(const juce::String& outputDeviceName,
                                      const juce::String& inputDeviceName) override;
};
}

