#include "FengYinEngineAudioIODevice.h"

#if defined(_WIN32)
#include <avrt.h>
#endif

#include <chrono>
#include <cmath>

namespace fengyin::audioengine
{
namespace
{
constexpr auto engineTypeName = "FengYin Audio Engine";
constexpr auto engineDeviceName = "FengYin Low Latency Output";
}

FengYinEngineAudioIODevice::FengYinEngineAudioIODevice()
    : AudioIODevice(engineDeviceName, engineTypeName)
{
}

FengYinEngineAudioIODevice::~FengYinEngineAudioIODevice()
{
    close();
}

juce::StringArray FengYinEngineAudioIODevice::getOutputChannelNames() { return { "Left", "Right" }; }
juce::StringArray FengYinEngineAudioIODevice::getInputChannelNames() { return {}; }
std::optional<juce::BigInteger> FengYinEngineAudioIODevice::getDefaultOutputChannels() const
{
    juce::BigInteger result; result.setRange(0, 2, true); return result;
}
std::optional<juce::BigInteger> FengYinEngineAudioIODevice::getDefaultInputChannels() const
{
    return juce::BigInteger();
}
juce::Array<double> FengYinEngineAudioIODevice::getAvailableSampleRates() { return { 48000.0 }; }
juce::Array<int> FengYinEngineAudioIODevice::getAvailableBufferSizes() { return { 128, 256, 512 }; }
int FengYinEngineAudioIODevice::getDefaultBufferSize() { return 256; }

juce::String FengYinEngineAudioIODevice::open(const juce::BigInteger& inputChannels,
                                               const juce::BigInteger& outputChannels,
                                               double sampleRate, int bufferSizeSamples)
{
    close();
    if (! inputChannels.isZero()) return "FengYin engine does not accept audio input";
    if (outputChannels.countNumberOfSetBits() == 0) return "No output channels selected";
    if (std::abs(sampleRate - engineSampleRate) > 0.5) return "FengYin engine requires 48000 Hz";
    if (bufferSizeSamples != 128 && bufferSizeSamples != 256 && bufferSizeSamples != 512)
        return "Unsupported FengYin engine buffer size";
#if defined(_WIN32)
    std::wstring mappingError;
    if (! mapping.createOrOpen(instrumentRegionName, mappingError))
        return juce::String(mappingError.c_str());
    requestSemaphore = CreateSemaphoreW(nullptr, 0, 4, instrumentRequestSemaphoreName);
    if (requestSemaphore == nullptr)
    {
        mapping.close();
        return "Cannot create FengYin engine request semaphore";
    }
    producer = std::make_unique<AudioBlockProducer>(*mapping.get());
    bufferFrames = bufferSizeSamples;
    xruns.store(0, std::memory_order_relaxed);
    opened.store(true, std::memory_order_release);
    error.clear();
    return {};
#else
    (void) bufferSizeSamples;
    return "FengYin engine is only available on Windows";
#endif
}

void FengYinEngineAudioIODevice::close()
{
    stop();
#if defined(_WIN32)
    if (requestSemaphore != nullptr) CloseHandle(requestSemaphore);
    requestSemaphore = nullptr;
#endif
    producer.reset();
    mapping.close();
    opened.store(false, std::memory_order_release);
}

bool FengYinEngineAudioIODevice::isOpen() { return opened.load(std::memory_order_acquire); }

void FengYinEngineAudioIODevice::start(juce::AudioIODeviceCallback* callback)
{
    stop();
    if (! isOpen() || callback == nullptr) return;
    activeCallback = callback;
    stopRequested.store(false, std::memory_order_release);
    callback->audioDeviceAboutToStart(this);
    playing.store(true, std::memory_order_release);
    pump = std::thread([this] { run(); });
}

void FengYinEngineAudioIODevice::stop()
{
    stopRequested.store(true, std::memory_order_release);
#if defined(_WIN32)
    if (requestSemaphore != nullptr) ReleaseSemaphore(requestSemaphore, 1, nullptr);
#endif
    if (pump.joinable()) pump.join();
    playing.store(false, std::memory_order_release);
    if (activeCallback != nullptr) activeCallback->audioDeviceStopped();
    activeCallback = nullptr;
}

bool FengYinEngineAudioIODevice::isPlaying() { return playing.load(std::memory_order_acquire); }
juce::String FengYinEngineAudioIODevice::getLastError() { return error; }
int FengYinEngineAudioIODevice::getCurrentBufferSizeSamples() { return bufferFrames; }
double FengYinEngineAudioIODevice::getCurrentSampleRate() { return engineSampleRate; }
int FengYinEngineAudioIODevice::getCurrentBitDepth() { return 32; }
juce::BigInteger FengYinEngineAudioIODevice::getActiveOutputChannels() const
{
    juce::BigInteger result; if (opened.load()) result.setRange(0, 2, true); return result;
}
juce::BigInteger FengYinEngineAudioIODevice::getActiveInputChannels() const { return {}; }
int FengYinEngineAudioIODevice::getOutputLatencyInSamples() { return bufferFrames * 2; }
int FengYinEngineAudioIODevice::getInputLatencyInSamples() { return 0; }
int FengYinEngineAudioIODevice::getXRunCount() const noexcept { return xruns.load(); }

void FengYinEngineAudioIODevice::run() noexcept
{
#if defined(_WIN32)
    DWORD taskIndex = 0;
    auto* mmcss = AvSetMmThreadCharacteristicsW(L"Pro Audio", &taskIndex);
    std::array<float, maximumFramesPerBlock> left {};
    std::array<float, maximumFramesPerBlock> right {};
    float* outputs[] { left.data(), right.data() };
    juce::AudioIODeviceCallbackContext context {};
    while (! stopRequested.load(std::memory_order_acquire))
    {
        const auto wait = WaitForSingleObject(requestSemaphore, 1000);
        if (stopRequested.load(std::memory_order_acquire)) break;
        if (wait == WAIT_TIMEOUT) continue;
        if (wait != WAIT_OBJECT_0) { error = "FengYin engine request wait failed"; break; }
        juce::FloatVectorOperations::clear(left.data(), bufferFrames);
        juce::FloatVectorOperations::clear(right.data(), bufferFrames);
        if (activeCallback != nullptr)
            activeCallback->audioDeviceIOCallbackWithContext(nullptr, 0, outputs, 2, bufferFrames, context);
        const float* sources[] { left.data(), right.data() };
        const auto qpc = static_cast<std::uint64_t>(juce::Time::getHighResolutionTicks());
        if (producer == nullptr || ! producer->tryPush(sources, 2, static_cast<std::uint32_t>(bufferFrames), qpc))
            xruns.fetch_add(1, std::memory_order_relaxed);
    }
    if (mmcss != nullptr) AvRevertMmThreadCharacteristics(mmcss);
#endif
    playing.store(false, std::memory_order_release);
}

FengYinEngineAudioIODeviceType::FengYinEngineAudioIODeviceType()
    : AudioIODeviceType(engineTypeName)
{
}

juce::StringArray FengYinEngineAudioIODeviceType::getDeviceNames(bool wantInputNames) const
{
    return wantInputNames ? juce::StringArray() : juce::StringArray { engineDeviceName };
}
int FengYinEngineAudioIODeviceType::getDefaultDeviceIndex(bool forInput) const { return forInput ? -1 : 0; }
int FengYinEngineAudioIODeviceType::getIndexOfDevice(juce::AudioIODevice* device, bool asInput) const
{
    return ! asInput && device != nullptr && device->getTypeName() == engineTypeName ? 0 : -1;
}
juce::AudioIODevice* FengYinEngineAudioIODeviceType::createDevice(const juce::String& outputDeviceName,
                                                                   const juce::String& inputDeviceName)
{
    if (outputDeviceName != engineDeviceName || inputDeviceName.isNotEmpty()) return nullptr;
    return new FengYinEngineAudioIODevice();
}
}
