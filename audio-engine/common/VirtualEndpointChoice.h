#pragma once
#include <string>
#include <algorithm>
#include <cwctype>
namespace fengyin::audioengine
{
inline bool vbCableTrial()
{
#if defined(_WIN32)
    return true; // Production route: no launcher or environment variable required.
#else
    return false;
#endif
}
inline bool matchesVirtualEndpoint(std::wstring name, bool cable, bool capture = false)
{
    std::transform(name.begin(), name.end(), name.begin(), [](wchar_t c) { return std::towlower(c); });
    if (!cable) return name.find(L"fengyin") != std::wstring::npos || name.find(L"风吟共享扬声器") != std::wstring::npos;
    // Only the base cable, never CABLE-A/B, Voicemeeter, or a physical microphone.
    const auto baseCable = name.find(L"vb-audio virtual cable") != std::wstring::npos;
    const auto multiChannel = name.find(L"16 ch") != std::wstring::npos;
    // Enumeration already restricts data flow. Accept localised/renamed stereo
    // endpoints, but never the separate 16-channel cable or additional A/B cables.
    return baseCable && !multiChannel
        && (capture ? name.find(L"cable input") == std::wstring::npos
                    : name.find(L"cable output") == std::wstring::npos);
}
}
