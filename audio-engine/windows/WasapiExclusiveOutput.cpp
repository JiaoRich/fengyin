#include "WasapiExclusiveOutput.h"
#include "NamedSharedAudioRegion.h"
#include "PhysicalOutputSelector.h"

#if defined(_WIN32)
#include <avrt.h>
#include <propkeydef.h>
#include <functiondiscoverykeys_devpkey.h>
#include <ksmedia.h>
#endif

#include <algorithm>
#include <chrono>
#include <cmath>

namespace fengyin::audioengine
{
#if defined(_WIN32)
namespace
{
enum class PhysicalSampleFormat { float32, pcm16 };

WAVEFORMATEXTENSIBLE makeStereoFormat(PhysicalSampleFormat format)
{
    WAVEFORMATEXTENSIBLE result {};
    result.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
    result.Format.nChannels = engineChannels;
    result.Format.nSamplesPerSec = engineSampleRate;
    result.Format.wBitsPerSample = format == PhysicalSampleFormat::float32 ? 32 : 16;
    result.Format.nBlockAlign = result.Format.nChannels * result.Format.wBitsPerSample / 8;
    result.Format.nAvgBytesPerSec = result.Format.nSamplesPerSec * result.Format.nBlockAlign;
    result.Format.cbSize = sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX);
    result.Samples.wValidBitsPerSample = result.Format.wBitsPerSample;
    result.dwChannelMask = SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT;
    result.SubFormat = format == PhysicalSampleFormat::float32
        ? KSDATAFORMAT_SUBTYPE_IEEE_FLOAT : KSDATAFORMAT_SUBTYPE_PCM;
    return result;
}

void interleaveOutput(BYTE* destination, PhysicalSampleFormat format,
                      const float* left, const float* right, UINT32 frames) noexcept
{
    if (format == PhysicalSampleFormat::float32)
    {
        auto* samples = reinterpret_cast<float*>(destination);
        for (UINT32 frame = 0; frame < frames; ++frame)
        {
            samples[frame * 2] = left[frame];
            samples[frame * 2 + 1] = right[frame];
        }
        return;
    }
    auto* samples = reinterpret_cast<std::int16_t*>(destination);
    for (UINT32 frame = 0; frame < frames; ++frame)
    {
        const auto convert = [] (float value)
        {
            return static_cast<std::int16_t>(std::lrint(std::clamp(value, -1.0f, 1.0f) * 32767.0f));
        };
        samples[frame * 2] = convert(left[frame]);
        samples[frame * 2 + 1] = convert(right[frame]);
    }
}
}
#endif

WasapiExclusiveOutput::~WasapiExclusiveOutput()
{
    stop();
}

bool WasapiExclusiveOutput::start(AudioEngineCore& engine, std::uint32_t requestedFrames,
                                  const std::wstring& preferredEndpointId,
                                  std::wstring& error)
{
    stop();
    if (requestedFrames != 128 && requestedFrames != 256 && requestedFrames != 512)
    {
        error = L"Unsupported buffer size";
        return false;
    }
    {
        std::lock_guard lock(startMutex);
        startReported = false;
        startSucceeded = false;
        startMessage.clear();
    }
    stopRequested.store(false, std::memory_order_release);
    worker = std::thread([this, &engine, requestedFrames, preferredEndpointId]
    {
        run(&engine, requestedFrames, preferredEndpointId);
    });
    std::unique_lock lock(startMutex);
    if (! startCondition.wait_for(lock, std::chrono::seconds(5), [this] { return startReported; }))
    {
        error = L"Audio engine start timed out";
        lock.unlock();
        stop();
        return false;
    }
    error = startMessage;
    const auto success = startSucceeded;
    lock.unlock();
    if (! success) stop();
    return success;
}

void WasapiExclusiveOutput::stop() noexcept
{
    stopRequested.store(true, std::memory_order_release);
    if (worker.joinable()) worker.join();
    running.store(false, std::memory_order_release);
    activeFrames.store(0, std::memory_order_release);
#if defined(_WIN32)
    if (instrumentRequest != nullptr)
    {
        CloseHandle(instrumentRequest);
        instrumentRequest = nullptr;
    }
#endif
}

void WasapiExclusiveOutput::reportStarted(bool success, const std::wstring& message,
                                          std::uint32_t frames) noexcept
{
    {
        std::lock_guard lock(startMutex);
        startSucceeded = success;
        startMessage = message;
        startReported = true;
        activeFrames.store(success ? frames : 0, std::memory_order_release);
    }
    startCondition.notify_all();
}

void WasapiExclusiveOutput::run(AudioEngineCore* engine, std::uint32_t requestedFrames,
                                std::wstring preferredEndpointId) noexcept
{
#if defined(_WIN32)
    const auto coResult = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(coResult))
    {
        reportStarted(false, L"COM initialization failed", 0);
        return;
    }

    Microsoft::WRL::ComPtr<IMMDeviceEnumerator> enumerator;
    Microsoft::WRL::ComPtr<IMMDevice> device;
    Microsoft::WRL::ComPtr<IAudioClient> client;
    Microsoft::WRL::ComPtr<IAudioRenderClient> renderer;
    HANDLE audioEvent = nullptr;
    HANDLE mmcss = nullptr;
    auto finish = [&]
    {
        if (client) client->Stop();
        if (mmcss != nullptr) AvRevertMmThreadCharacteristics(mmcss);
        if (audioEvent != nullptr) CloseHandle(audioEvent);
        running.store(false, std::memory_order_release);
        CoUninitialize();
    };

    auto hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                               IID_PPV_ARGS(&enumerator));
    std::wstring selectedId, selectedName, selectionError;
    if (SUCCEEDED(hr) && ! selectPhysicalOutput(*enumerator.Get(), preferredEndpointId,
                                                device, selectedId, selectedName, selectionError))
        hr = HRESULT_FROM_WIN32(ERROR_NOT_FOUND);
    if (SUCCEEDED(hr)) hr = device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, &client);
    if (FAILED(hr))
    {
        reportStarted(false, L"Cannot open the default physical output", 0);
        finish();
        return;
    }

    auto sampleFormat = PhysicalSampleFormat::float32;
    auto format = makeStereoFormat(sampleFormat);
    if (client->IsFormatSupported(AUDCLNT_SHAREMODE_EXCLUSIVE, &format.Format, nullptr) != S_OK)
    {
        sampleFormat = PhysicalSampleFormat::pcm16;
        format = makeStereoFormat(sampleFormat);
        if (client->IsFormatSupported(AUDCLNT_SHAREMODE_EXCLUSIVE, &format.Format, nullptr) != S_OK)
        {
            reportStarted(false, L"The physical output does not support 48 kHz stereo in exclusive mode", 0);
            finish();
            return;
        }
    }

    auto requestedDuration = static_cast<REFERENCE_TIME>(
        (10000000ull * requestedFrames + engineSampleRate - 1) / engineSampleRate);
    audioEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    instrumentRequest = CreateSemaphoreW(nullptr, 0, 4, instrumentRequestSemaphoreName);
    if (audioEvent == nullptr || instrumentRequest == nullptr)
    {
        reportStarted(false, L"Cannot create audio event", 0);
        finish();
        return;
    }
    hr = client->Initialize(AUDCLNT_SHAREMODE_EXCLUSIVE,
                            AUDCLNT_STREAMFLAGS_EVENTCALLBACK | AUDCLNT_STREAMFLAGS_NOPERSIST,
                            requestedDuration, requestedDuration, &format.Format, nullptr);
    if (hr == AUDCLNT_E_BUFFER_SIZE_NOT_ALIGNED)
    {
        UINT32 alignedFrames = 0;
        if (SUCCEEDED(client->GetBufferSize(&alignedFrames)) && alignedFrames > 0
            && alignedFrames <= maximumFramesPerBlock)
        {
            renderer.Reset();
            client.Reset();
            hr = device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, &client);
            requestedDuration = static_cast<REFERENCE_TIME>(
                (10000000ull * alignedFrames + engineSampleRate - 1) / engineSampleRate);
            if (SUCCEEDED(hr))
                hr = client->Initialize(AUDCLNT_SHAREMODE_EXCLUSIVE,
                                        AUDCLNT_STREAMFLAGS_EVENTCALLBACK | AUDCLNT_STREAMFLAGS_NOPERSIST,
                                        requestedDuration, requestedDuration, &format.Format, nullptr);
        }
    }
    if (FAILED(hr))
    {
        reportStarted(false, L"The requested exclusive buffer is not supported", 0);
        finish();
        return;
    }
    UINT32 actualFrames = 0;
    hr = client->GetBufferSize(&actualFrames);
    if (SUCCEEDED(hr)) hr = client->SetEventHandle(audioEvent);
    if (SUCCEEDED(hr)) hr = client->GetService(IID_PPV_ARGS(&renderer));
    if (FAILED(hr) || actualFrames == 0 || actualFrames > maximumFramesPerBlock)
    {
        reportStarted(false, L"Invalid physical output buffer", 0);
        finish();
        return;
    }

    DWORD taskIndex = 0;
    mmcss = AvSetMmThreadCharacteristicsW(L"Pro Audio", &taskIndex);
    std::array<float, maximumFramesPerBlock> left {};
    std::array<float, maximumFramesPerBlock> right {};
    float* planes[] { left.data(), right.data() };
    engine->render(planes, 2, actualFrames);
    BYTE* bytes = nullptr;
    hr = renderer->GetBuffer(actualFrames, &bytes);
    if (SUCCEEDED(hr))
    {
        interleaveOutput(bytes, sampleFormat, left.data(), right.data(), actualFrames);
        hr = renderer->ReleaseBuffer(actualFrames, 0);
    }
    if (SUCCEEDED(hr)) hr = client->Start();
    if (FAILED(hr))
    {
        reportStarted(false, L"Cannot start physical output", 0);
        finish();
        return;
    }

    running.store(true, std::memory_order_release);
    reportStarted(true, L"", actualFrames);
    ReleaseSemaphore(instrumentRequest, 2, nullptr);
    while (! stopRequested.load(std::memory_order_acquire))
    {
        const auto wait = WaitForSingleObject(audioEvent, 1000);
        if (wait == WAIT_TIMEOUT) continue;
        if (wait != WAIT_OBJECT_0) break;
        engine->render(planes, 2, actualFrames);
        bytes = nullptr;
        if (FAILED(renderer->GetBuffer(actualFrames, &bytes))) break;
        interleaveOutput(bytes, sampleFormat, left.data(), right.data(), actualFrames);
        if (FAILED(renderer->ReleaseBuffer(actualFrames, 0))) break;
        ReleaseSemaphore(instrumentRequest, 1, nullptr);
    }
    finish();
#else
    (void) engine;
    (void) requestedFrames;
    (void) preferredEndpointId;
    reportStarted(false, L"WASAPI is only available on Windows", 0);
#endif
}
}
