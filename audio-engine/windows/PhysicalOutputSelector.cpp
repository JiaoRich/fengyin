#include "PhysicalOutputSelector.h"

#if defined(_WIN32)
#include <windows.h>
#include <propsys.h>
#include <propkeydef.h>
#include <functiondiscoverykeys_devpkey.h>
#include <propvarutil.h>

#include <algorithm>
#include <cwctype>
#include <vector>
#endif

namespace fengyin::audioengine
{
#if defined(_WIN32)
namespace
{
struct EndpointCandidate
{
    Microsoft::WRL::ComPtr<IMMDevice> device;
    std::wstring id;
    std::wstring name;
    bool isDefault = false;
};

std::wstring lower(std::wstring value)
{
    std::transform(value.begin(), value.end(), value.begin(), [] (wchar_t character)
    {
        return static_cast<wchar_t>(std::towlower(character));
    });
    return value;
}

bool isFengYinVirtualEndpoint(const std::wstring& id, const std::wstring& name)
{
    const auto combined = lower(id + L" " + name);
    return combined.find(L"fengyin") != std::wstring::npos
        || combined.find(L"风吟共享扬声器") != std::wstring::npos;
}

bool readEndpoint(IMMDevice& device, EndpointCandidate& result)
{
    LPWSTR rawId = nullptr;
    if (FAILED(device.GetId(&rawId)) || rawId == nullptr)
        return false;
    result.id = rawId;
    CoTaskMemFree(rawId);

    Microsoft::WRL::ComPtr<IPropertyStore> properties;
    if (SUCCEEDED(device.OpenPropertyStore(STGM_READ, &properties)))
    {
        PROPVARIANT value;
        PropVariantInit(&value);
        if (SUCCEEDED(properties->GetValue(PKEY_Device_FriendlyName, &value))
            && value.vt == VT_LPWSTR && value.pwszVal != nullptr)
            result.name = value.pwszVal;
        PropVariantClear(&value);
    }
    result.device = &device;
    return true;
}
}

bool selectPhysicalOutput(IMMDeviceEnumerator& enumerator,
                          const std::wstring& preferredEndpointId,
                          Microsoft::WRL::ComPtr<IMMDevice>& selected,
                          std::wstring& selectedId,
                          std::wstring& selectedName,
                          std::wstring& error) noexcept
{
    selected.Reset();
    selectedId.clear();
    selectedName.clear();
    error.clear();

    std::wstring defaultId;
    Microsoft::WRL::ComPtr<IMMDevice> defaultDevice;
    if (SUCCEEDED(enumerator.GetDefaultAudioEndpoint(eRender, eMultimedia, &defaultDevice)))
    {
        EndpointCandidate candidate;
        if (readEndpoint(*defaultDevice.Get(), candidate)) defaultId = candidate.id;
    }

    Microsoft::WRL::ComPtr<IMMDeviceCollection> collection;
    if (FAILED(enumerator.EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &collection))
        || collection == nullptr)
    {
        error = L"Cannot enumerate physical audio outputs";
        return false;
    }

    UINT count = 0;
    if (FAILED(collection->GetCount(&count)))
    {
        error = L"Cannot read physical audio output count";
        return false;
    }

    std::vector<EndpointCandidate> candidates;
    for (UINT index = 0; index < count; ++index)
    {
        Microsoft::WRL::ComPtr<IMMDevice> device;
        EndpointCandidate candidate;
        if (FAILED(collection->Item(index, &device)) || device == nullptr
            || ! readEndpoint(*device.Get(), candidate)
            || isFengYinVirtualEndpoint(candidate.id, candidate.name))
            continue;
        candidate.isDefault = candidate.id == defaultId;
        candidates.push_back(std::move(candidate));
    }
    if (candidates.empty())
    {
        error = L"No usable physical audio output is active";
        return false;
    }

    const auto preferred = std::find_if(candidates.begin(), candidates.end(), [&] (const auto& item)
    {
        return ! preferredEndpointId.empty() && item.id == preferredEndpointId;
    });
    const auto systemDefault = std::find_if(candidates.begin(), candidates.end(), [] (const auto& item)
    {
        return item.isDefault;
    });
    const auto chosen = preferred != candidates.end() ? preferred
        : systemDefault != candidates.end() ? systemDefault : candidates.begin();
    selected = chosen->device;
    selectedId = chosen->id;
    selectedName = chosen->name;
    return true;
}
#endif
}
