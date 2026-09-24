#pragma once
#include <juce_core/juce_core.h>
#if JUCE_WINDOWS
#include <windows.h>
#include <setupapi.h>
#endif

namespace fengyin
{
// Physical media devices and their driver versions, NOT MMDevice endpoints.
// An analogue headphone jack changing the default endpoint must not invalidate a test.
inline juce::String audioHardwareIdentity()
{
   #if JUCE_WINDOWS
    const GUID mediaClass { 0x4d36e96c, 0xe325, 0x11ce, {0xbf,0xc1,0x08,0x00,0x2b,0xe1,0x03,0x18} };
    auto devices = SetupDiGetClassDevsW(&mediaClass, nullptr, nullptr, DIGCF_PRESENT);
    if (devices == INVALID_HANDLE_VALUE) return {};
    juce::StringArray identities;
    SP_DEVINFO_DATA info {};
    info.cbSize = sizeof(info);
    for (DWORD index = 0; SetupDiEnumDeviceInfo(devices, index, &info); ++index)
    {
        wchar_t id[2048] {};
        if (! SetupDiGetDeviceInstanceIdW(devices, &info, id, 2048, nullptr)) continue;
        wchar_t version[256] {};
        auto key = SetupDiOpenDevRegKey(devices, &info, DICS_FLAG_GLOBAL, 0, DIREG_DRV, KEY_READ);
        if (key != INVALID_HANDLE_VALUE)
        {
            DWORD bytes = sizeof(version) - sizeof(wchar_t), type = 0;
            RegQueryValueExW(key, L"DriverVersion", nullptr, &type,
                            reinterpret_cast<LPBYTE>(version), &bytes);
            RegCloseKey(key);
        }
        identities.add(juce::String(id) + "|" + juce::String(version));
    }
    SetupDiDestroyDeviceInfoList(devices);
    identities.sort(false);
    return identities.isEmpty() ? juce::String() : juce::String::toHexString(identities.joinIntoString("\n").hashCode64());
   #else
    return "non-windows";
   #endif
}
}
