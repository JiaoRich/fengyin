#pragma once
#if defined(_WIN32)
#include <windows.h>
#include <shlobj.h>
#include <string>
namespace fengyin::audioengine {
// Per-user lock, shared by login recovery and the lifetime of a routed engine.
class RouteOwnership {
public:
    RouteOwnership() {
        PWSTR path = nullptr;
        if (FAILED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &path))) return;
        std::wstring name = L"Local\\FengYinRoute-";
        for (const auto* p = path; *p; ++p) name += (*p == L'\\' || *p == L'/') ? L'_' : *p;
        CoTaskMemFree(path);
        handle = CreateMutexW(nullptr, FALSE, name.c_str());
        if (handle) {
            const auto status = WaitForSingleObject(handle, 0);
            owned = status == WAIT_OBJECT_0 || status == WAIT_ABANDONED;
        }
    }
    ~RouteOwnership() { if (owned) ReleaseMutex(handle); if (handle) CloseHandle(handle); }
    bool acquired() const { return owned; }
    RouteOwnership(const RouteOwnership&) = delete;
    RouteOwnership& operator=(const RouteOwnership&) = delete;
private:
    HANDLE handle = nullptr;
    bool owned = false;
};
}
#endif
