#include "DefaultEndpointRouter.h"

#if defined(_WIN32)
#include <functiondiscoverykeys_devpkey.h>
#include <mmdeviceapi.h>
#include <propidl.h>
#include <propvarutil.h>
#include <wrl/client.h>

#include <algorithm>
#include <cwctype>
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

    Microsoft::WRL::ComPtr<IPolicyConfig> policy;
    hr = CoCreateInstance(policyConfigClient, nullptr, CLSCTX_ALL,
                          __uuidof(IPolicyConfig), reinterpret_cast<void**>(policy.GetAddressOf()));
    if (FAILED(hr) || ! policy)
    {
        error = L"Windows refused endpoint routing control";
        restore();
        return false;
    }
    active = true;
    for (const auto role : roles)
    {
        hr = policy->SetDefaultEndpoint(virtualId.c_str(), role);
        if (FAILED(hr))
        {
            error = L"Cannot route Windows audio to FengYin Shared Speaker";
            restore();
            return false;
        }
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
        Microsoft::WRL::ComPtr<IPolicyConfig> policy;
        if (SUCCEEDED(CoCreateInstance(policyConfigClient, nullptr, CLSCTX_ALL,
                                       __uuidof(IPolicyConfig),
                                       reinterpret_cast<void**>(policy.GetAddressOf()))) && policy)
            for (std::size_t index = 0; index < roles.size(); ++index)
                if (! previousEndpointIds[index].empty())
                    (void) policy->SetDefaultEndpoint(previousEndpointIds[index].c_str(), roles[index]);
    }
#endif
    active = false;
    previousEndpointIds = {};
#if defined(_WIN32)
    if (comInitialised) CoUninitialize();
#endif
    comInitialised = false;
}
}
