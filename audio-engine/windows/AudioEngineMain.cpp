#include "AudioEngineCore.h"
#include "DefaultEndpointRouter.h"
#include "NamedSharedAudioRegion.h"
#include "WasapiExclusiveOutput.h"
#include "WasapiLoopbackInput.h"

#if defined(_WIN32)
#include <windows.h>
#include <shellapi.h>
#endif

#include <cstdint>
#include <cstdlib>
#include <string>
#include <vector>

using namespace fengyin::audioengine;

#if defined(_WIN32)
namespace
{
bool launchRecoveryWatchdog()
{
    std::wstring executable(32768, L'\0');
    const auto length = GetModuleFileNameW(nullptr, executable.data(),
                                           static_cast<DWORD>(executable.size()));
    if (length == 0 || length >= executable.size()) return false;
    executable.resize(length);
    const auto slash = executable.find_last_of(L"\\/");
    const auto watchdog = executable.substr(0, slash + 1) + L"FengYinAudioWatchdog.exe";
    auto command = L"\"" + watchdog + L"\" --watch-pid " + std::to_wstring(GetCurrentProcessId());
    std::vector<wchar_t> mutableCommand(command.begin(), command.end());
    mutableCommand.push_back(L'\0');
    STARTUPINFOW startup {};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process {};
    if (! CreateProcessW(nullptr, mutableCommand.data(), nullptr, nullptr, FALSE,
                         CREATE_NO_WINDOW | CREATE_UNICODE_ENVIRONMENT,
                         nullptr, nullptr, &startup, &process))
        return false;
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    return true;
}

void discardQueuedAudio(SharedAudioRegion& region) noexcept
{
    region.readSequence.store(region.writeSequence.load(std::memory_order_acquire),
                              std::memory_order_release);
}

void publishState(SharedAudioRegion& instrument, SharedAudioRegion& system,
                  StreamState state, std::uint32_t frames) noexcept
{
    instrument.activePeriodFrames.store(frames, std::memory_order_release);
    system.activePeriodFrames.store(frames, std::memory_order_release);
    instrument.state.store(static_cast<std::uint32_t>(state), std::memory_order_release);
    system.state.store(static_cast<std::uint32_t>(state), std::memory_order_release);
}
}

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int)
{
    std::wstring preferredEndpointId;
    std::uint32_t requestedFrames = 256;
    bool routeSystemAudio = false;
    int argumentCount = 0;
    if (auto** arguments = CommandLineToArgvW(GetCommandLineW(), &argumentCount))
    {
        for (int index = 1; index < argumentCount; ++index)
            if (std::wstring(arguments[index]) == L"--physical-endpoint")
            {
                if (index + 1 < argumentCount) preferredEndpointId = arguments[++index];
            }
            else if (std::wstring(arguments[index]) == L"--buffer")
            {
                if (index + 1 < argumentCount)
                {
                    const auto parsed = std::wcstoul(arguments[++index], nullptr, 10);
                    if (parsed == 128 || parsed == 256 || parsed == 512)
                        requestedFrames = static_cast<std::uint32_t>(parsed);
                }
            }
            else if (std::wstring(arguments[index]) == L"--route-system-audio")
                routeSystemAudio = true;
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
    DefaultEndpointRouter router;
    if (routeSystemAudio)
    {
        // Start recovery before changing any Windows endpoint. If this process
        // dies in the small interval between journal creation and the route
        // becoming active, the watchdog still observes our exit and restores
        // the journal instead of leaving Windows pointed at a dead endpoint.
        if (! launchRecoveryWatchdog())
        {
            CloseHandle(stopEvent);
            ReleaseMutex(singleton);
            CloseHandle(singleton);
            return 4;
        }
        std::wstring previousPhysicalEndpoint;
        if (! router.routeSystemAudioToFengYin(previousPhysicalEndpoint, error))
        {
            CloseHandle(stopEvent);
            ReleaseMutex(singleton);
            CloseHandle(singleton);
            return 5;
        }
        if (preferredEndpointId.empty()) preferredEndpointId = previousPhysicalEndpoint;
    }
    WasapiExclusiveOutput output;
    // Try the user's/automatic low-latency request first, then only the larger
    // safe periods. This happens before the JUCE graph opens, so the graph is
    // created once at the physical period that actually succeeded.
    std::vector<std::uint32_t> periodCandidates { requestedFrames };
    for (const auto candidate : { 128u, 256u, 512u })
        if (candidate > requestedFrames) periodCandidates.push_back(candidate);
    bool outputStarted = false;
    for (const auto candidate : periodCandidates)
    {
        // Existing shared clients need a short moment to migrate after the
        // Windows default was moved to the virtual endpoint. Retry the same
        // low period before increasing latency; this is startup work, never
        // executed on the real-time audio callback.
        for (int attempt = 0; attempt < 3 && ! outputStarted; ++attempt)
        {
            if (output.start(core, candidate, preferredEndpointId, error))
            {
                requestedFrames = candidate;
                outputStarted = true;
            }
            else if (attempt < 2)
            {
                Sleep(200);
            }
        }
        if (outputStarted) break;
    }
    if (! outputStarted)
    {
        lifecycle.useFallback();
        instrumentMapping.get()->state.store(static_cast<std::uint32_t>(StreamState::fallback));
        systemMapping.get()->state.store(static_cast<std::uint32_t>(StreamState::fallback));
        CloseHandle(stopEvent);
        ReleaseMutex(singleton);
        CloseHandle(singleton);
        return 6;
    }

    lifecycle.markRunning();
    const auto actualFrames = output.actualBufferFrames();
    publishState(*instrumentMapping.get(), *systemMapping.get(), StreamState::running, actualFrames);
    WasapiLoopbackInput loopback;
    std::wstring loopbackError;
    // The engineering fast path remains usable before the signed virtual
    // speaker is installed. Once present, its Windows mix joins automatically.
    const auto loopbackStarted = loopback.start(*systemMapping.get(), loopbackError);
    if (routeSystemAudio && ! loopbackStarted)
    {
        output.stop();
        router.restore();
        CloseHandle(stopEvent);
        ReleaseMutex(singleton);
        CloseHandle(singleton);
        return 7;
    }
    // Device removal, sleep and driver resets invalidate an exclusive stream.
    // Recover inside the engine process without rebuilding the VST graph. The
    // existing fast-path client remains valid when the physical period is the
    // same; if a replacement device requires another period we fail closed so
    // the main app can reopen its audio device rather than drift or crackle.
    bool unrecoverableOutputFailure = false;
    while (WaitForSingleObject(stopEvent, 250) == WAIT_TIMEOUT)
    {
        if (routeSystemAudio)
        {
            std::wstring newPhysicalEndpoint, routeError;
            if (router.pollPhysicalDefaultChange(newPhysicalEndpoint, routeError)
                && ! newPhysicalEndpoint.empty() && newPhysicalEndpoint != preferredEndpointId)
            {
                lifecycle.beginRecovery();
                publishState(*instrumentMapping.get(), *systemMapping.get(), StreamState::recovering, 0);
                output.stop();
                discardQueuedAudio(*instrumentMapping.get());
                discardQueuedAudio(*systemMapping.get());
                core.reset();
                preferredEndpointId = newPhysicalEndpoint;
                std::wstring restartError;
                if (output.start(core, requestedFrames, preferredEndpointId, restartError)
                    && output.actualBufferFrames() == actualFrames)
                {
                    publishState(*instrumentMapping.get(), *systemMapping.get(), StreamState::running,
                                 actualFrames);
                    lifecycle.markRunning();
                    continue;
                }
                output.stop();
                unrecoverableOutputFailure = true;
                lifecycle.useFallback();
                publishState(*instrumentMapping.get(), *systemMapping.get(), StreamState::fallback, 0);
                break;
            }
        }
        if (output.isRunning())
        {
            if (! routeSystemAudio || loopback.isRunning()) continue;

            // A Windows Audio service restart can invalidate only the virtual
            // loopback side while the physical instrument stream remains
            // healthy. Reopen that side independently so SWAM never reloads.
            loopback.stop();
            discardQueuedAudio(*systemMapping.get());
            bool loopbackRecovered = false;
            for (int attempt = 0; attempt < 6
                 && WaitForSingleObject(stopEvent, 0) != WAIT_OBJECT_0; ++attempt)
            {
                std::wstring restartError;
                if (loopback.start(*systemMapping.get(), restartError))
                {
                    loopbackRecovered = true;
                    break;
                }
                if (WaitForSingleObject(stopEvent, 750) == WAIT_OBJECT_0) break;
            }
            if (loopbackRecovered) continue;
            unrecoverableOutputFailure = true;
            lifecycle.useFallback();
            publishState(*instrumentMapping.get(), *systemMapping.get(), StreamState::fallback, 0);
            break;
        }
        lifecycle.beginRecovery();
        publishState(*instrumentMapping.get(), *systemMapping.get(), StreamState::recovering, 0);
        output.stop();
        discardQueuedAudio(*instrumentMapping.get());
        discardQueuedAudio(*systemMapping.get());
        core.reset();

        bool recovered = false;
        for (int attempt = 0; attempt < 6 && WaitForSingleObject(stopEvent, 0) != WAIT_OBJECT_0; ++attempt)
        {
            std::wstring restartError;
            if (output.start(core, requestedFrames, preferredEndpointId, restartError))
            {
                const auto recoveredFrames = output.actualBufferFrames();
                if (recoveredFrames == actualFrames)
                {
                    publishState(*instrumentMapping.get(), *systemMapping.get(), StreamState::running,
                                 recoveredFrames);
                    lifecycle.markRunning();
                    recovered = true;
                    break;
                }
                output.stop();
            }
            if (WaitForSingleObject(stopEvent, 750) == WAIT_OBJECT_0) break;
        }
        if (! recovered)
        {
            unrecoverableOutputFailure = true;
            lifecycle.useFallback();
            publishState(*instrumentMapping.get(), *systemMapping.get(), StreamState::fallback, 0);
            break;
        }
    }
    lifecycle.stop();
    loopback.stop();
    output.stop();
    if (! unrecoverableOutputFailure)
        publishState(*instrumentMapping.get(), *systemMapping.get(), StreamState::stopped, 0);
    CloseHandle(stopEvent);
    ReleaseMutex(singleton);
    CloseHandle(singleton);
    return unrecoverableOutputFailure ? 8 : 0;
}
#else
int main() { return 0; }
#endif
