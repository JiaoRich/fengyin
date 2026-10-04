#include "WasapiExclusiveOutput.h"
#include "NamedSharedAudioRegion.h"

#if defined(_WIN32)
#include <avrt.h>
#include <functiondiscoverykeys_devpkey.h>
#endif

#include <algorithm>
#include <chrono>

namespace fengyin::audioengine
{
WasapiExclusiveOutput::~WasapiExclusiveOutput()
{
    stop();
}

bool WasapiExclusiveOutput::start(AudioEngineCore& engine, std::uint32_t requestedFrames,
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
    worker = std::thread([this, &engine, requestedFrames] { run(&engine, requestedFrames); });
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

void WasapiExclusiveOutput::run(AudioEngineCore* engine, std::uint32_t requestedFrames) noexcept
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
    if (SUCCEEDED(hr)) hr = enumerator->GetDefaultAudioEndpoint(eRender, eMultimedia, &device);
    if (SUCCEEDED(hr)) hr = device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, &client);
    if (FAILED(hr))
    {
        reportStarted(false, L"Cannot open the default physical output", 0);
        finish();
        return;
    }

    WAVEFORMATEX format {};
    format.wFormatTag = WAVE_FORMAT_IEEE_FLOAT;
    format.nChannels = engineChannels;
    format.nSamplesPerSec = engineSampleRate;
    format.wBitsPerSample = 32;
    format.nBlockAlign = format.nChannels * format.wBitsPerSample / 8;
    format.nAvgBytesPerSec = format.nSamplesPerSec * format.nBlockAlign;
    if (client->IsFormatSupported(AUDCLNT_SHAREMODE_EXCLUSIVE, &format, nullptr) != S_OK)
    {
        reportStarted(false, L"The physical output does not support 48 kHz stereo float in exclusive mode", 0);
        finish();
        return;
    }

    const auto requestedDuration = static_cast<REFERENCE_TIME>(
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
                            requestedDuration, requestedDuration, &format, nullptr);
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
        auto* interleaved = reinterpret_cast<float*>(bytes);
        for (UINT32 frame = 0; frame < actualFrames; ++frame)
        {
            interleaved[frame * 2] = left[frame];
            interleaved[frame * 2 + 1] = right[frame];
        }
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
        auto* interleaved = reinterpret_cast<float*>(bytes);
        for (UINT32 frame = 0; frame < actualFrames; ++frame)
        {
            interleaved[frame * 2] = left[frame];
            interleaved[frame * 2 + 1] = right[frame];
        }
        if (FAILED(renderer->ReleaseBuffer(actualFrames, 0))) break;
        ReleaseSemaphore(instrumentRequest, 1, nullptr);
    }
    finish();
#else
    (void) engine;
    (void) requestedFrames;
    reportStarted(false, L"WASAPI is only available on Windows", 0);
#endif
}
}
