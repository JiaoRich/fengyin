#include "NamedSharedAudioRegion.h"

#include <new>

namespace fengyin::audioengine
{
NamedSharedAudioRegion::~NamedSharedAudioRegion()
{
    close();
}

bool NamedSharedAudioRegion::createOrOpen(const std::wstring& name, std::wstring& error) noexcept
{
    close();
#if defined(_WIN32)
    mapping = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0,
                                 static_cast<DWORD>(sizeof(SharedAudioRegion)), name.c_str());
    if (mapping == nullptr)
    {
        error = L"CreateFileMapping failed: " + std::to_wstring(GetLastError());
        return false;
    }
    created = GetLastError() != ERROR_ALREADY_EXISTS;
    auto* memory = MapViewOfFile(mapping, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(SharedAudioRegion));
    if (memory == nullptr)
    {
        error = L"MapViewOfFile failed: " + std::to_wstring(GetLastError());
        close();
        return false;
    }
    region = static_cast<SharedAudioRegion*>(memory);
    if (created)
    {
        new (region) SharedAudioRegion();
        initialiseRegion(*region);
    }
    else if (! isCompatible(region->protocol))
    {
        error = L"Shared audio protocol version mismatch";
        close();
        return false;
    }
    return true;
#else
    (void) name;
    error = L"Named shared audio regions are only available on Windows";
    return false;
#endif
}

void NamedSharedAudioRegion::close() noexcept
{
#if defined(_WIN32)
    if (region != nullptr) UnmapViewOfFile(region);
    if (mapping != nullptr) CloseHandle(mapping);
    mapping = nullptr;
#endif
    region = nullptr;
    created = false;
}
}

