#include "DefaultEndpointRouter.h"

#if defined(_WIN32)
#include <windows.h>
#include <shellapi.h>

#include <cstdlib>
#include <string>

using namespace fengyin::audioengine;

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int)
{
    DWORD watchPid = 0;
    bool recoverOnly = false;
    int argumentCount = 0;
    if (auto** arguments = CommandLineToArgvW(GetCommandLineW(), &argumentCount))
    {
        for (int index = 1; index < argumentCount; ++index)
            if (std::wstring(arguments[index]) == L"--watch-pid" && index + 1 < argumentCount)
                watchPid = std::wcstoul(arguments[++index], nullptr, 10);
            else if (std::wstring(arguments[index]) == L"--recover-only")
                recoverOnly = true;
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
    return DefaultEndpointRouter::restorePendingRoute(error) ? 0 : 2;
}
#else
int main() { return 0; }
#endif
