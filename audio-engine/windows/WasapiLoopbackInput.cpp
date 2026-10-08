#include "WasapiLoopbackInput.h"
#include "../common/VirtualEndpointChoice.h"

#if defined(_WIN32)
#include <windows.h>
#include <audioclient.h>
#include <avrt.h>
#include <propsys.h>
#include <propkeydef.h>
#include <functiondiscoverykeys_devpkey.h>
#include <ksmedia.h>
#include <mmdeviceapi.h>
#include <propvarutil.h>
#include <wrl/client.h>

#include <algorithm>
#include <cwctype>
#endif

#include <array>
#include <chrono>

namespace fengyin::audioengine
{
#if defined(_WIN32)
namespace
{
bool isFengYinSpeaker(IMMDevice& device)
{
    Microsoft::WRL::ComPtr<IPropertyStore> properties;
    if (FAILED(device.OpenPropertyStore(STGM_READ, &properties))) return false;
    PROPVARIANT value;
    PropVariantInit(&value);
    std::wstring name;
    if (SUCCEEDED(properties->GetValue(PKEY_Device_FriendlyName, &value))
        && value.vt == VT_LPWSTR && value.pwszVal != nullptr)
        name = value.pwszVal;
    PropVariantClear(&value);
    std::transform(name.begin(), name.end(), name.begin(), [] (wchar_t character)
    {
        return static_cast<wchar_t>(std::towlower(character));
    });
    return matchesVirtualEndpoint(name, vbCableTrial(), vbCableTrial());
}

enum class SampleEncoding
{
    unsupported,
    float32,
    pcm16
};

SampleEncoding getSampleEncoding(const WAVEFORMATEX& format)
{
    if (format.nSamplesPerSec != engineSampleRate || format.nChannels != engineChannels)
        return SampleEncoding::unsupported;
    if (format.wFormatTag == WAVE_FORMAT_IEEE_FLOAT && format.wBitsPerSample == 32)
        return SampleEncoding::float32;
    if (format.wFormatTag == WAVE_FORMAT_PCM && format.wBitsPerSample == 16)
        return SampleEncoding::pcm16;
    if (format.wFormatTag != WAVE_FORMAT_EXTENSIBLE
        || format.cbSize < sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX))
        return SampleEncoding::unsupported;
    const auto& extensible = reinterpret_cast<const WAVEFORMATEXTENSIBLE&>(format);
    if (extensible.SubFormat == KSDATAFORMAT_SUBTYPE_IEEE_FLOAT && format.wBitsPerSample == 32)
        return SampleEncoding::float32;
    if (extensible.SubFormat == KSDATAFORMAT_SUBTYPE_PCM && format.wBitsPerSample == 16)
        return SampleEncoding::pcm16;
    return SampleEncoding::unsupported;
}

Microsoft::WRL::ComPtr<IMMDevice> findFengYinSpeaker(IMMDeviceEnumerator& enumerator)
{
    Microsoft::WRL::ComPtr<IMMDeviceCollection> collection;
    if (FAILED(enumerator.EnumAudioEndpoints(vbCableTrial() ? eCapture : eRender, DEVICE_STATE_ACTIVE, &collection))) return {};
    UINT count = 0;
    if (FAILED(collection->GetCount(&count))) return {};
    for (UINT index = 0; index < count; ++index)
    {
        Microsoft::WRL::ComPtr<IMMDevice> device;
        if (SUCCEEDED(collection->Item(index, &device)) && device && isFengYinSpeaker(*device.Get()))
            return device;
    }
    return {};
}
}
#endif

WasapiLoopbackInput::~WasapiLoopbackInput()
{
    stop();
}

bool WasapiLoopbackInput::start(SharedAudioRegion& destination, std::wstring& error)
{
    stop();
#if defined(_WIN32)
    startedEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (startedEvent == nullptr)
    {
        error = L"Cannot create loopback start event";
        return false;
    }
    startError.clear();
    stopRequested.store(false, std::memory_order_release);
    worker = std::thread([this, &destination] { run(&destination); });
    if (WaitForSingleObject(startedEvent, 5000) != WAIT_OBJECT_0)
    {
        error = L"Virtual speaker capture start timed out";
        stop();
        return false;
    }
    error = startError;
    if (! running.load(std::memory_order_acquire))
    {
        stop();
        return false;
    }
    return true;
#else
    (void) destination;
    error = L"Virtual speaker capture is only available on Windows";
    return false;
#endif
}

void WasapiLoopbackInput::stop() noexcept
{
    stopRequested.store(true, std::memory_order_release);
    if (worker.joinable()) worker.join();
    running.store(false, std::memory_order_release);
#if defined(_WIN32)
    if (startedEvent != nullptr) CloseHandle(startedEvent);
    startedEvent = nullptr;
#endif
}

void WasapiLoopbackInput::run(SharedAudioRegion* destination) noexcept
{
#if defined(_WIN32)
    const auto com = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    Microsoft::WRL::ComPtr<IMMDeviceEnumerator> enumerator;
    Microsoft::WRL::ComPtr<IMMDevice> device;
    Microsoft::WRL::ComPtr<IAudioClient> client;
    Microsoft::WRL::ComPtr<IAudioCaptureClient> capture;
    HANDLE audioEvent = nullptr;
    HANDLE mmcss = nullptr;
    WAVEFORMATEX* mixFormat = nullptr;
    auto fail = [&] (const wchar_t* message)
    {
        if (destination != nullptr)
            destination->producerActive.store(0, std::memory_order_release);
        startError = message;
        running.store(false, std::memory_order_release);
        if (startedEvent != nullptr) SetEvent(startedEvent);
    };
    auto finish = [&]
    {
        if (destination != nullptr)
            destination->producerActive.store(0, std::memory_order_release);
        if (client) client->Stop();
        if (mixFormat != nullptr) CoTaskMemFree(mixFormat);
        if (mmcss != nullptr) AvRevertMmThreadCharacteristics(mmcss);
        if (audioEvent != nullptr) CloseHandle(audioEvent);
        running.store(false, std::memory_order_release);
        capture.Reset();
        client.Reset();
        device.Reset();
        enumerator.Reset();
        if (SUCCEEDED(com)) CoUninitialize();
    };
    if (FAILED(com)) { fail(L"Cannot initialize loopback COM"); return; }
    auto hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                               IID_PPV_ARGS(&enumerator));
    if (SUCCEEDED(hr)) device = findFengYinSpeaker(*enumerator.Get());
    if (! device) hr = HRESULT_FROM_WIN32(ERROR_NOT_FOUND);
    if (SUCCEEDED(hr)) hr = device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, &client);
    if (SUCCEEDED(hr)) hr = client->GetMixFormat(&mixFormat);
    // Convert only the browser capture stream, never the ASIO instrument stream.
    WAVEFORMATEX cableFormat {};
    cableFormat.wFormatTag = WAVE_FORMAT_IEEE_FLOAT;
    cableFormat.nChannels = engineChannels;
    cableFormat.nSamplesPerSec = engineSampleRate;
    cableFormat.wBitsPerSample = 32;
    cableFormat.nBlockAlign = engineChannels * sizeof(float);
    cableFormat.nAvgBytesPerSec = engineSampleRate * cableFormat.nBlockAlign;
    const auto* streamFormat = vbCableTrial() ? &cableFormat : mixFormat;
    const auto encoding = streamFormat != nullptr ? getSampleEncoding(*streamFormat)
                                               : SampleEncoding::unsupported;
    if (FAILED(hr) || encoding == SampleEncoding::unsupported)
    {
        fail(vbCableTrial() ? L"Cannot open VB-CABLE CABLE Output. Install the base VB-CABLE and enable its recording endpoint." : L"FengYin virtual speaker format must be 48 kHz stereo PCM");
        finish();
        return;
    }
    audioEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    if (audioEvent == nullptr)
    {
        fail(L"Cannot create virtual speaker capture event");
        finish();
        return;
    }
    hr = client->Initialize(AUDCLNT_SHAREMODE_SHARED,
                            (vbCableTrial() ? (AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM | AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY) : AUDCLNT_STREAMFLAGS_LOOPBACK) | AUDCLNT_STREAMFLAGS_EVENTCALLBACK
                                | AUDCLNT_STREAMFLAGS_NOPERSIST,
                            0, 0, streamFormat, nullptr);
    if (SUCCEEDED(hr)) hr = client->SetEventHandle(audioEvent);
    if (SUCCEEDED(hr)) hr = client->GetService(IID_PPV_ARGS(&capture));
    if (SUCCEEDED(hr)) hr = client->Start();
    if (FAILED(hr))
    {
        const auto reason = hr == AUDCLNT_E_DEVICE_IN_USE
            ? L" Audio endpoint is already in use; check exclusive/ASIO ownership."
            : hr == E_ACCESSDENIED ? L" Access denied; check Windows microphone access for desktop apps."
                                  : L"";
        const auto message = std::wstring(vbCableTrial() ? L"Cannot start VB-CABLE capture." : L"Cannot start FengYin virtual speaker capture")
            + reason
            + L" HRESULT=" + std::to_wstring(static_cast<unsigned long>(hr));
        fail(message.c_str());
        finish();
        return;
    }

    DWORD taskIndex = 0;
    mmcss = AvSetMmThreadCharacteristicsW(L"Pro Audio", &taskIndex);
    AudioBlockProducer producer(*destination);
    std::array<float, maximumFramesPerBlock> silence {};
    destination->producerActive.store(1, std::memory_order_release);
    running.store(true, std::memory_order_release);
    SetEvent(startedEvent);
    while (! stopRequested.load(std::memory_order_acquire))
    {
        const auto wait = WaitForSingleObject(audioEvent, 250);
        if (wait == WAIT_TIMEOUT) continue;
        if (wait != WAIT_OBJECT_0) break;
        UINT32 packetFrames = 0;
        while (SUCCEEDED(capture->GetNextPacketSize(&packetFrames)) && packetFrames > 0)
        {
            BYTE* bytes = nullptr;
            DWORD flags = 0;
            UINT64 devicePosition = 0, qpc = 0;
            if (FAILED(capture->GetBuffer(&bytes, &packetFrames, &flags, &devicePosition, &qpc))) break;
            auto remaining = packetFrames;
            auto offset = 0u;
            while (remaining > 0)
            {
                const auto frames = std::min<std::uint32_t>(remaining, maximumFramesPerBlock);
                const float* channels[2] {};
                std::array<float, maximumFramesPerBlock> left {}, right {};
                if ((flags & AUDCLNT_BUFFERFLAGS_SILENT) != 0 || bytes == nullptr)
                {
                    channels[0] = silence.data();
                    channels[1] = silence.data();
                }
                else
                {
                    if (encoding == SampleEncoding::float32)
                    {
                        const auto* interleaved = reinterpret_cast<const float*>(bytes);
                        for (std::uint32_t frame = 0; frame < frames; ++frame)
                        {
                            left[frame] = interleaved[(offset + frame) * 2];
                            right[frame] = interleaved[(offset + frame) * 2 + 1];
                        }
                    }
                    else
                    {
                        const auto* interleaved = reinterpret_cast<const std::int16_t*>(bytes);
                        constexpr auto scale = 1.0f / 32768.0f;
                        for (std::uint32_t frame = 0; frame < frames; ++frame)
                        {
                            left[frame] = static_cast<float>(interleaved[(offset + frame) * 2]) * scale;
                            right[frame] = static_cast<float>(interleaved[(offset + frame) * 2 + 1]) * scale;
                        }
                    }
                    channels[0] = left.data();
                    channels[1] = right.data();
                }
                producer.tryPush(channels, 2, frames, qpc,
                                 (flags & AUDCLNT_BUFFERFLAGS_DATA_DISCONTINUITY) != 0
                                     ? blockDiscontinuity : blockNone);
                remaining -= frames;
                offset += frames;
            }
            capture->ReleaseBuffer(packetFrames);
        }
    }
    destination->producerActive.store(0, std::memory_order_release);
    finish();
#else
    (void) destination;
#endif
}
}
