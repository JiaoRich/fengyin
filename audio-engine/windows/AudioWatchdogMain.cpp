#include "DefaultEndpointRouter.h"

#if defined(_WIN32)
#include <windows.h>
#include <shellapi.h>

#include <cstdlib>
#include <string>
#include <filesystem>
#include <fstream>

using namespace fengyin::audioengine;

static void logRecovery(const std::wstring& message)
{
    PWSTR raw = nullptr;
    if (FAILED(SHGetKnownFolderPath(FOLDERID_LocalAppData, KF_FLAG_CREATE, nullptr, &raw))) return;
    const auto folder = std::filesystem::path(raw) / L"FengYin";
    CoTaskMemFree(raw);
    std::error_code ignored;
    std::filesystem::create_directories(folder, ignored);
    std::wofstream stream(folder / L"audio-route-recovery.log", std::ios::app);
    SYSTEMTIME now {}; GetLocalTime(&now);
    stream << now.wYear << L'-' << now.wMonth << L'-' << now.wDay << L' '
        << now.wHour << L':' << now.wMinute << L':' << now.wSecond << L' ' << message << L'\n';
}

static bool engineRunning()
{
    // Also recognises pre-1.1.7 engines which did not acquire RouteOwnership.
    const auto mutex = OpenMutexW(SYNCHRONIZE | MUTEX_MODIFY_STATE, FALSE,
                                  L"Local\\FengYin.AudioEngine.Singleton.v2");
    if (!mutex) return GetLastError() == ERROR_ACCESS_DENIED;
    const auto result = WaitForSingleObject(mutex, 0);
    if (result == WAIT_OBJECT_0 || result == WAIT_ABANDONED) ReleaseMutex(mutex);
    CloseHandle(mutex);
    return result != WAIT_OBJECT_0 && result != WAIT_ABANDONED;
}

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int)
{
    DWORD watchPid = 0;
    bool recoverOnly = false;
    bool loginRecovery = false;
    bool snapshotInstall = false;
    int argumentCount = 0;
    if (auto** arguments = CommandLineToArgvW(GetCommandLineW(), &argumentCount))
    {
        for (int index = 1; index < argumentCount; ++index)
            if (std::wstring(arguments[index]) == L"--watch-pid" && index + 1 < argumentCount)
                watchPid = std::wcstoul(arguments[++index], nullptr, 10);
            else if (std::wstring(arguments[index]) == L"--recover-only")
                recoverOnly = true;
            else if (std::wstring(arguments[index]) == L"--login-recovery")
                loginRecovery = recoverOnly = true;
            else if (std::wstring(arguments[index]) == L"--snapshot-install")
                snapshotInstall = recoverOnly = true;
        LocalFree(arguments);
    }
    if (! recoverOnly && watchPid != 0)
    {
        if (const auto process = OpenProcess(SYNCHRONIZE, FALSE, watchPid))
        {
            (void) WaitForSingleObject(process, INFINITE);
            CloseHandle(process);
        }
    }
    std::wstring error;
    if (snapshotInstall) {
        if (engineRunning()) { logRecovery(L"Installation blocked: close running engine"); return 3; }
        const auto ok = DefaultEndpointRouter::snapshotBeforeDependencyInstall(error);
        logRecovery(ok ? L"Pre-install defaults recorded" : error);
        return ok ? 0 : 2;
    }
    if (loginRecovery) {
        // Bounded, non-resident recovery; a running bridge holds RouteOwnership.
        for (int attempt = 0; attempt < 30; ++attempt) {
            if (engineRunning()) { logRecovery(L"Login recovery deferred to running engine"); return 0; }
            if (DefaultEndpointRouter::restorePendingRoute(error)) {
                logRecovery(L"Login recovery completed (or no owned route)"); return 0;
            }
            Sleep(1000);
        }
        logRecovery(error);
        return 2; // Preserve journal for the next launch/login.
    }
    return DefaultEndpointRouter::restorePendingRoute(error) ? 0 : 2;
}
#else
int main() { return 0; }
#endif
