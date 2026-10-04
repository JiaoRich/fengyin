#include "DefaultEndpointRouter.h"

#if defined(_WIN32)
#include <propkeydef.h>
#include <functiondiscoverykeys_devpkey.h>
#include <mmdeviceapi.h>
#include <propidl.h>
#include <propvarutil.h>
#include <shlobj.h>
#include <wrl/client.h>

#include <algorithm>
#include <cwctype>
#include <cstdint>
#include <vector>
#endif

namespace fengyin::audioengine
{
#if defined(_WIN32)
namespace
{
struct DeviceShareMode;

interface DECLSPEC_UUID("F8679F50-850A-41CF-9C72-430F290290C8") IPolicyConfig
    : public IUnknown
{
    virtual HRESULT STDMETHODCALLTYPE GetMixFormat(PCWSTR, WAVEFORMATEX**) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetDeviceFormat(PCWSTR, INT, WAVEFORMATEX**) = 0;
    virtual HRESULT STDMETHODCALLTYPE ResetDeviceFormat(PCWSTR) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetDeviceFormat(PCWSTR, WAVEFORMATEX*, WAVEFORMATEX*) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetProcessingPeriod(PCWSTR, INT, PINT64, PINT64) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetProcessingPeriod(PCWSTR, PINT64) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetShareMode(PCWSTR, DeviceShareMode*) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetShareMode(PCWSTR, DeviceShareMode*) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetPropertyValue(PCWSTR, const PROPERTYKEY&, PROPVARIANT*) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetPropertyValue(PCWSTR, const PROPERTYKEY&, PROPVARIANT*) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetDefaultEndpoint(PCWSTR, ERole) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetEndpointVisibility(PCWSTR, INT) = 0;
};

const CLSID policyConfigClient =
    { 0x870af99c, 0x171d, 0x4f9e, { 0xaf, 0x0d, 0xe6, 0x3d, 0xf4, 0x0c, 0x2b, 0xc9 } };

constexpr std::array<ERole, 3> roles { eConsole, eMultimedia, eCommunications };
constexpr std::uint32_t journalMagic = 0x46594152; // FYAR
constexpr std::uint32_t journalVersion = 1;

struct JournalHeader
{
    std::uint32_t magic = journalMagic;
    std::uint32_t version = journalVersion;
    std::array<std::uint32_t, 3> lengths {};
};

std::wstring journalPath()
{
    PWSTR localAppData = nullptr;
    if (FAILED(SHGetKnownFolderPath(FOLDERID_LocalAppData, KF_FLAG_CREATE, nullptr, &localAppData))
        || localAppData == nullptr)
        return {};
    std::wstring folder(localAppData);
    CoTaskMemFree(localAppData);
    folder += L"\\FengYin";
    (void) CreateDirectoryW(folder.c_str(), nullptr);
    return folder + L"\\audio-route-recovery.bin";
}

bool writeJournal(const std::array<std::wstring, 3>& endpointIds)
{
    const auto path = journalPath();
    if (path.empty()) return false;
    JournalHeader header;
    for (std::size_t index = 0; index < endpointIds.size(); ++index)
    {
        if (endpointIds[index].empty() || endpointIds[index].size() > 32768) return false;
        header.lengths[index] = static_cast<std::uint32_t>(endpointIds[index].size());
    }
    const auto file = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                                  FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_NOT_CONTENT_INDEXED, nullptr);
    if (file == INVALID_HANDLE_VALUE) return false;
    DWORD written = 0;
    auto ok = WriteFile(file, &header, sizeof(header), &written, nullptr) && written == sizeof(header);
    for (std::size_t index = 0; ok && index < endpointIds.size(); ++index)
    {
        const auto bytes = header.lengths[index] * static_cast<DWORD>(sizeof(wchar_t));
        ok = WriteFile(file, endpointIds[index].data(), bytes, &written, nullptr) && written == bytes;
    }
    if (ok) ok = FlushFileBuffers(file) != FALSE;
    CloseHandle(file);
    if (! ok) (void) DeleteFileW(path.c_str());
    return ok;
}

bool readJournal(std::array<std::wstring, 3>& endpointIds)
{
    endpointIds = {};
    const auto path = journalPath();
    if (path.empty()) return false;
    const auto file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                                  FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return false;
    JournalHeader header;
    DWORD read = 0;
    auto ok = ReadFile(file, &header, sizeof(header), &read, nullptr) && read == sizeof(header)
           && header.magic == journalMagic && header.version == journalVersion;
    for (std::size_t index = 0; ok && index < endpointIds.size(); ++index)
    {
        if (header.lengths[index] == 0 || header.lengths[index] > 32768) { ok = false; break; }
        endpointIds[index].resize(header.lengths[index]);
        const auto bytes = header.lengths[index] * static_cast<DWORD>(sizeof(wchar_t));
        ok = ReadFile(file, endpointIds[index].data(), bytes, &read, nullptr) && read == bytes;
    }
    CloseHandle(file);
    return ok;
}

void deleteJournal()
{
    const auto path = journalPath();
    if (! path.empty()) (void) DeleteFileW(path.c_str());
}

bool setEndpoints(const std::array<std::wstring, 3>& endpointIds)
{
    Microsoft::WRL::ComPtr<IPolicyConfig> policy;
    if (FAILED(CoCreateInstance(policyConfigClient, nullptr, CLSCTX_ALL,
                                __uuidof(IPolicyConfig),
                                reinterpret_cast<void**>(policy.GetAddressOf()))) || ! policy)
        return false;
    auto ok = true;
    for (std::size_t index = 0; index < roles.size(); ++index)
        if (endpointIds[index].empty()
            || FAILED(policy->SetDefaultEndpoint(endpointIds[index].c_str(), roles[index])))
            ok = false;
    return ok;
}

std::wstring lower(std::wstring value)
{
    std::transform(value.begin(), value.end(), value.begin(), [] (wchar_t character)
    {
        return static_cast<wchar_t>(std::towlower(character));
    });
    return value;
}

bool readDevice(IMMDevice& device, std::wstring& id, std::wstring& name)
{
    LPWSTR rawId = nullptr;
    if (FAILED(device.GetId(&rawId)) || rawId == nullptr) return false;
    id = rawId;
    CoTaskMemFree(rawId);
    Microsoft::WRL::ComPtr<IPropertyStore> properties;
    if (SUCCEEDED(device.OpenPropertyStore(STGM_READ, &properties)))
    {
        PROPVARIANT value;
        PropVariantInit(&value);
        if (SUCCEEDED(properties->GetValue(PKEY_Device_FriendlyName, &value))
            && value.vt == VT_LPWSTR && value.pwszVal != nullptr)
            name = value.pwszVal;
        PropVariantClear(&value);
    }
    return true;
}

bool isVirtual(const std::wstring& id, const std::wstring& name)
{
    const auto combined = lower(id + L" " + name);
    return combined.find(L"fengyin") != std::wstring::npos
        || combined.find(L"风吟共享扬声器") != std::wstring::npos;
}

std::wstring getId(IMMDeviceEnumerator& enumerator, ERole role)
{
    Microsoft::WRL::ComPtr<IMMDevice> device;
    if (FAILED(enumerator.GetDefaultAudioEndpoint(eRender, role, &device)) || ! device) return {};
    std::wstring id, name;
    return readDevice(*device.Get(), id, name) ? id : std::wstring {};
}

std::wstring findVirtual(IMMDeviceEnumerator& enumerator)
{
    Microsoft::WRL::ComPtr<IMMDeviceCollection> collection;
    if (FAILED(enumerator.EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &collection)) || ! collection)
        return {};
    UINT count = 0;
    if (FAILED(collection->GetCount(&count))) return {};
    for (UINT index = 0; index < count; ++index)
    {
        Microsoft::WRL::ComPtr<IMMDevice> device;
        std::wstring id, name;
        if (SUCCEEDED(collection->Item(index, &device)) && device
            && readDevice(*device.Get(), id, name) && isVirtual(id, name))
            return id;
    }
    return {};
}

std::wstring findFirstPhysical(IMMDeviceEnumerator& enumerator)
{
    Microsoft::WRL::ComPtr<IMMDeviceCollection> collection;
    if (FAILED(enumerator.EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &collection)) || ! collection)
        return {};
    UINT count = 0;
    if (FAILED(collection->GetCount(&count))) return {};
    for (UINT index = 0; index < count; ++index)
    {
        Microsoft::WRL::ComPtr<IMMDevice> device;
        std::wstring id, name;
        if (SUCCEEDED(collection->Item(index, &device)) && device
            && readDevice(*device.Get(), id, name) && ! isVirtual(id, name))
            return id;
    }
    return {};
}

bool isActiveEndpoint(IMMDeviceEnumerator& enumerator, const std::wstring& id)
{
    Microsoft::WRL::ComPtr<IMMDevice> device;
    DWORD state = 0;
    return ! id.empty() && SUCCEEDED(enumerator.GetDevice(id.c_str(), &device)) && device
        && SUCCEEDED(device->GetState(&state)) && (state & DEVICE_STATE_ACTIVE) != 0;
}
}
#endif

DefaultEndpointRouter::~DefaultEndpointRouter()
{
    restore();
}

bool DefaultEndpointRouter::routeSystemAudioToFengYin(std::wstring& physicalEndpointId,
                                                       std::wstring& error) noexcept
{
    restore();
#if defined(_WIN32)
    if (! restorePendingRoute(error)) return false;
    const auto com = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(com) && com != RPC_E_CHANGED_MODE)
    {
        error = L"Cannot initialise endpoint routing COM";
        return false;
    }
    comInitialised = SUCCEEDED(com);
    Microsoft::WRL::ComPtr<IMMDeviceEnumerator> enumerator;
    auto hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                               IID_PPV_ARGS(&enumerator));
    if (FAILED(hr) || ! enumerator)
    {
        error = L"Cannot enumerate Windows audio endpoints";
        restore();
        return false;
    }
    const auto virtualId = findVirtual(*enumerator.Get());
    if (virtualId.empty())
    {
        error = L"FengYin Shared Speaker is not installed";
        restore();
        return false;
    }
    for (std::size_t index = 0; index < roles.size(); ++index)
        previousEndpointIds[index] = getId(*enumerator.Get(), roles[index]);
    physicalEndpointId = previousEndpointIds[1].empty() ? previousEndpointIds[0]
                                                        : previousEndpointIds[1];
    if (physicalEndpointId.empty() || physicalEndpointId == virtualId)
    {
        error = L"Cannot determine the previous physical audio output";
        restore();
        return false;
    }

    if (! writeJournal(previousEndpointIds))
    {
        error = L"Cannot create the audio endpoint recovery journal";
        restore();
        return false;
    }
    active = true;
    std::array<std::wstring, 3> virtualEndpoints { virtualId, virtualId, virtualId };
    if (! setEndpoints(virtualEndpoints))
    {
        error = L"Cannot route Windows audio to FengYin Shared Speaker";
        restore();
        return false;
    }
    return true;
#else
    (void) physicalEndpointId;
    error = L"Endpoint routing is only available on Windows";
    return false;
#endif
}

void DefaultEndpointRouter::restore() noexcept
{
#if defined(_WIN32)
    if (active)
    {
        if (setEndpoints(previousEndpointIds)) deleteJournal();
    }
#endif
    active = false;
    previousEndpointIds = {};
#if defined(_WIN32)
    if (comInitialised) CoUninitialize();
#endif
    comInitialised = false;
}

bool DefaultEndpointRouter::restorePendingRoute(std::wstring& error) noexcept
{
#if defined(_WIN32)
    std::array<std::wstring, 3> endpointIds;
    if (! readJournal(endpointIds)) return true;
    const auto com = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(com) && com != RPC_E_CHANGED_MODE)
    {
        error = L"Cannot initialise recovery COM";
        return false;
    }
    Microsoft::WRL::ComPtr<IMMDeviceEnumerator> enumerator;
    auto restored = SUCCEEDED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                                               IID_PPV_ARGS(&enumerator))) && enumerator;
    if (restored)
    {
        const auto fallback = findFirstPhysical(*enumerator.Get());
        if (fallback.empty())
            restored = false;
        else
            for (auto& id : endpointIds)
                if (! isActiveEndpoint(*enumerator.Get(), id)) id = fallback;
    }
    if (restored) restored = setEndpoints(endpointIds);
    if (restored) deleteJournal();
    if (SUCCEEDED(com)) CoUninitialize();
    if (! restored) error = L"Cannot restore the previous Windows audio endpoint";
    return restored;
#else
    (void) error;
    return true;
#endif
}
}
