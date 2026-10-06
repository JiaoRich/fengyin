#pragma once
#include <string>
#include <algorithm>
#include <cwctype>
#if defined(_WIN32)
#include <windows.h>
#endif
namespace fengyin::audioengine
{
inline bool vbCableTrial()
{
#if defined(_WIN32)
    wchar_t value[8] {};
    return GetEnvironmentVariableW(L"FENGYIN_VBCABLE_TEST", value, 8) == 1 && value[0] == L'1';
#else
    return false;
#endif
}
inline bool matchesVirtualEndpoint(std::wstring name, bool cable, bool capture = false)
{
    std::transform(name.begin(), name.end(), name.begin(), [](wchar_t c) { return std::towlower(c); });
    if (!cable) return name.find(L"fengyin") != std::wstring::npos || name.find(L"风吟共享扬声器") != std::wstring::npos;
    // Only the base cable, never CABLE-A/B, Voicemeeter, or a physical microphone.
    return name.find(capture ? L"cable output" : L"cable input") != std::wstring::npos
        && name.find(L"vb-audio virtual cable") != std::wstring::npos;
}
}
