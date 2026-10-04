#include "AudioEngineCore.h"
#include "NamedSharedAudioRegion.h"
#include "WasapiExclusiveOutput.h"

#if defined(_WIN32)
#include <windows.h>
#endif

#include <string>

using namespace fengyin::audioengine;

#if defined(_WIN32)
int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int)
{
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
    if (! output.start(core, 256, error))
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
    instrumentMapping.get()->state.store(static_cast<std::uint32_t>(StreamState::running));
    systemMapping.get()->state.store(static_cast<std::uint32_t>(StreamState::running));
    WaitForSingleObject(stopEvent, INFINITE);
    lifecycle.stop();
    output.stop();
    instrumentMapping.get()->state.store(static_cast<std::uint32_t>(StreamState::stopped));
    systemMapping.get()->state.store(static_cast<std::uint32_t>(StreamState::stopped));
    CloseHandle(stopEvent);
    ReleaseMutex(singleton);
    CloseHandle(singleton);
    return 0;
}
#else
int main() { return 0; }
#endif

