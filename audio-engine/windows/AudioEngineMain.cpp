#include "AudioEngineCore.h"
#include "NamedSharedAudioRegion.h"
#include "WasapiExclusiveOutput.h"

#if defined(_WIN32)
#include <windows.h>
#include <shellapi.h>
#endif

#include <cstdint>
#include <cstdlib>
#include <string>

using namespace fengyin::audioengine;

#if defined(_WIN32)
int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int)
{
    std::wstring preferredEndpointId;
    std::uint32_t requestedFrames = 256;
    int argumentCount = 0;
    if (auto** arguments = CommandLineToArgvW(GetCommandLineW(), &argumentCount))
    {
        for (int index = 1; index + 1 < argumentCount; ++index)
            if (std::wstring(arguments[index]) == L"--physical-endpoint")
                preferredEndpointId = arguments[++index];
            else if (std::wstring(arguments[index]) == L"--buffer")
            {
                const auto parsed = std::wcstoul(arguments[++index], nullptr, 10);
                if (parsed == 128 || parsed == 256 || parsed == 512)
                    requestedFrames = static_cast<std::uint32_t>(parsed);
            }
        LocalFree(arguments);
    }
    HANDLE singleton = CreateMutexW(nullptr, TRUE, engineSingletonName);
    if (singleton == nullptr || GetLastError() == ERROR_ALREADY_EXISTS)
    {
        if (singleton != nullptr) CloseHandle(singleton);
        return 0;
    }
    HANDLE stopEvent = CreateEventW(nullptr, TRUE, FALSE, engineStopEventName);
    if (stopEvent == nullptr)
    {
        CloseHandle(singleton);
        return 2;
    }

    NamedSharedAudioRegion instrumentMapping;
    NamedSharedAudioRegion systemMapping;
    std::wstring error;
    if (! instrumentMapping.createOrOpen(instrumentRegionName, error)
        || ! systemMapping.createOrOpen(systemRegionName, error))
    {
        CloseHandle(stopEvent);
        ReleaseMutex(singleton);
        CloseHandle(singleton);
        return 3;
    }

    EngineLifecycle lifecycle;
    lifecycle.beginStart();
    AudioEngineCore core(*instrumentMapping.get(), *systemMapping.get());
    WasapiExclusiveOutput output;
    if (! output.start(core, requestedFrames, preferredEndpointId, error))
    {
        lifecycle.useFallback();
        instrumentMapping.get()->state.store(static_cast<std::uint32_t>(StreamState::fallback));
        systemMapping.get()->state.store(static_cast<std::uint32_t>(StreamState::fallback));
        CloseHandle(stopEvent);
        ReleaseMutex(singleton);
        CloseHandle(singleton);
        return 4;
    }

    lifecycle.markRunning();
    const auto actualFrames = output.actualBufferFrames();
    instrumentMapping.get()->activePeriodFrames.store(actualFrames, std::memory_order_release);
    systemMapping.get()->activePeriodFrames.store(actualFrames, std::memory_order_release);
    instrumentMapping.get()->state.store(static_cast<std::uint32_t>(StreamState::running));
    systemMapping.get()->state.store(static_cast<std::uint32_t>(StreamState::running));
    WaitForSingleObject(stopEvent, INFINITE);
    lifecycle.stop();
    output.stop();
    instrumentMapping.get()->state.store(static_cast<std::uint32_t>(StreamState::stopped));
    systemMapping.get()->state.store(static_cast<std::uint32_t>(StreamState::stopped));
    instrumentMapping.get()->activePeriodFrames.store(0, std::memory_order_release);
    systemMapping.get()->activePeriodFrames.store(0, std::memory_order_release);
    CloseHandle(stopEvent);
    ReleaseMutex(singleton);
    CloseHandle(singleton);
    return 0;
}
#else
int main() { return 0; }
#endif
