#pragma once

#include "FengYinAudioProtocol.h"

#if defined(_WIN32)
#include <windows.h>
#endif

#include <string>

namespace fengyin::audioengine
{
class NamedSharedAudioRegion
{
public:
    NamedSharedAudioRegion() = default;
    ~NamedSharedAudioRegion();
    NamedSharedAudioRegion(const NamedSharedAudioRegion&) = delete;
    NamedSharedAudioRegion& operator=(const NamedSharedAudioRegion&) = delete;

    bool createOrOpen(const std::wstring& name, std::wstring& error) noexcept;
    void close() noexcept;
    [[nodiscard]] SharedAudioRegion* get() const noexcept { return region; }
    [[nodiscard]] bool wasCreated() const noexcept { return created; }

private:
#if defined(_WIN32)
    HANDLE mapping = nullptr;
#endif
    SharedAudioRegion* region = nullptr;
    bool created = false;
};

inline constexpr wchar_t instrumentRegionName[] = L"Local\\FengYin.Audio.Instrument.v2";
inline constexpr wchar_t systemRegionName[] = L"Local\\FengYin.Audio.System.v2";
inline constexpr wchar_t engineStopEventName[] = L"Local\\FengYin.AudioEngine.Stop.v2";
inline constexpr wchar_t engineSingletonName[] = L"Local\\FengYin.AudioEngine.Singleton.v2";
inline constexpr wchar_t instrumentRequestSemaphoreName[] = L"Local\\FengYin.Audio.Instrument.Request.v2";
}
