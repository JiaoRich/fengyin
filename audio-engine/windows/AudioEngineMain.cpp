#include "AudioEngineCore.h"
#include "DefaultEndpointRouter.h"
#include "NamedSharedAudioRegion.h"
#include "Asio4AllOutput.h"
#include "WasapiLoopbackInput.h"
#include "../common/VirtualEndpointChoice.h"

#if defined(_WIN32)
#include <windows.h>
#include <shellapi.h>
#include "AudioSessionDiagnostic.h"
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
    juce::ScopedJuceInitialiser_GUI juceInitialiser;
    const auto logFile = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("FengYin").getChildFile("audio-engine.log");
    logFile.getParentDirectory().createDirectory();
    juce::FileLogger logger(logFile, "ASIO4ALL bridge starting", 256 * 1024);
    juce::Logger::setCurrentLogger(&logger);
    juce::Logger::writeToLog(vbCableTrial() ? "SystemAudio=VB-CABLE capture (CABLE Output); route=CABLE Input" : "SystemAudio=FengYin speaker loopback");
    struct ResetLogger { ~ResetLogger() { juce::Logger::setCurrentLogger(nullptr); } } resetLogger;
    std::wstring preferredEndpointId;
    std::uint32_t requestedFrames = 256;
    bool routeSystemAudio = false;
    bool configureAsio = false;
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
            else if (std::wstring(arguments[index]) == L"--configure-asio")
                configureAsio = true;
        LocalFree(arguments);
    }
    const bool manualPhysicalOutput = !preferredEndpointId.empty();
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
    if (configureAsio)
    {
        std::unique_ptr<juce::AudioIODeviceType> asio(juce::AudioIODeviceType::createAudioIODeviceType_ASIO());
        bool shown = false;
        if (asio)
        {
            asio->scanForDevices();
            for (const auto& name : asio->getDeviceNames(false))
                if (name.containsIgnoreCase("ASIO4ALL"))
                {
                    std::unique_ptr<juce::AudioIODevice> device(asio->createDevice(name, {}));
                    if (device && device->hasControlPanel())
                    {
                        device->showControlPanel();
                        // ASIO controlPanel() may return while a modeless
                        // panel is still open. Keep the driver instance and
                        // message pump alive until its windows close.
                        juce::MessageManager::getInstance()->runDispatchLoopUntil(250);
                        for (;;)
                        {
                            bool visible = false;
                            EnumWindows([](HWND window, LPARAM context) -> BOOL
                            {
                                DWORD pid = 0;
                                GetWindowThreadProcessId(window, &pid);
                                if (pid == GetCurrentProcessId() && IsWindowVisible(window))
                                    *reinterpret_cast<bool*>(context) = true;
                                return TRUE;
                            }, reinterpret_cast<LPARAM>(&visible));
                            if (! visible || WaitForSingleObject(stopEvent, 0) == WAIT_OBJECT_0) break;
                            juce::MessageManager::getInstance()->runDispatchLoopUntil(20);
                        }
                        shown = true;
                    }
                    break;
                }
        }
        if (! shown) MessageBoxW(nullptr, L"未找到可用的 64 位 ASIO4ALL 驱动控制面板。", L"风吟 ASIO4ALL", MB_OK | MB_ICONERROR);
        CloseHandle(stopEvent);
        ReleaseMutex(singleton);
        CloseHandle(singleton);
        return shown ? 0 : 9;
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
            juce::Logger::writeToLog("Route failed: " + juce::String(error.c_str()));
            CloseHandle(stopEvent);
            ReleaseMutex(singleton);
            CloseHandle(singleton);
            return 5;
        }
        if (preferredEndpointId.empty()) preferredEndpointId = previousPhysicalEndpoint;
    }
    Asio4AllOutput output;
    // No WASAPI substitution or silent escalation to a larger period.
    const auto outputStarted = output.start(core, requestedFrames, preferredEndpointId, error, manualPhysicalOutput);
    if (! outputStarted)
    {
        logAudioSessions();
        juce::Logger::writeToLog("ASIO4ALL start failed: " + juce::String(error.c_str()));
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
    requestedFrames = actualFrames;
    instrumentMapping.get()->physicalOutputLatencyFrames.store(output.outputLatencyFrames());
    publishState(*instrumentMapping.get(), *systemMapping.get(), StreamState::running, actualFrames);
    WasapiLoopbackInput loopback;
    std::wstring loopbackError;
    // The engineering fast path remains usable before the signed virtual
    // speaker is installed. Once present, its Windows mix joins automatically.
    const auto loopbackStarted = loopback.start(*systemMapping.get(), loopbackError);
    if (routeSystemAudio && ! loopbackStarted)
    {
        juce::Logger::writeToLog("System capture failed: " + juce::String(loopbackError.c_str()));
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
    auto nextDiagnostic = juce::Time::getMillisecondCounterHiRes() + 5000.0;
    while (WaitForSingleObject(stopEvent, 0) == WAIT_TIMEOUT)
    {
        // JUCE's ASIO reset notifications use the message-thread timer.
        juce::MessageManager::getInstance()->runDispatchLoopUntil(20);
        if (instrumentMapping.get()->testToneRequest.exchange(0) != 0)
        {
            core.requestTestTone();
            juce::Logger::writeToLog("Diagnostic tone requested: 440Hz -30dBFS 2s; replaces mix during test");
        }
        if (juce::Time::getMillisecondCounterHiRes() >= nextDiagnostic)
        {
            const auto counters = core.getCounters();
            juce::Logger::writeToLog(juce::Time::getCurrentTime().toISO8601(true) + " Audio health: callbacks=" + juce::String(counters.renderCallbacks)
                + " instrumentUnderflows=" + juce::String(counters.instrumentUnderflows)
                + " systemUnderflows=" + juce::String(counters.systemUnderflows)
                + " clippedFrames=" + juce::String(counters.clippedFrames)
                + " producerPeak=" + juce::String(instrumentMapping.get()->producerPeakMicro.exchange(0) / 1000000.0, 6)
                + " instrumentReceivedPeak=" + juce::String(core.takeInstrumentPeak(), 6)
                + " systemReceivedPeak=" + juce::String(core.takeSystemPeak(), 6)
                + " asioPreOutputPeak=" + juce::String(core.takeOutputPeak(), 6)
                + " producerActive=" + juce::String(instrumentMapping.get()->producerActive.load())
                + " droppedBlocks=" + juce::String(instrumentMapping.get()->droppedBlocks.load()));
            nextDiagnostic += 5000.0;
            logAudioSessions();
        }
        if (routeSystemAudio)
        {
            std::wstring newPhysicalEndpoint, routeError;
            if (router.pollPhysicalDefaultChange(newPhysicalEndpoint, routeError)
                && !manualPhysicalOutput && ! newPhysicalEndpoint.empty() && newPhysicalEndpoint != preferredEndpointId)
            {
                const auto previous = preferredEndpointId;
                output.stop();
                if (output.start(core,requestedFrames,newPhysicalEndpoint,routeError,true))
                    preferredEndpointId=newPhysicalEndpoint;
                else
                {
                    juce::Logger::writeToLog("Endpoint switch failed; restoring previous output: " + juce::String(routeError.c_str()));
                    output.start(core,requestedFrames,previous,routeError);
                }
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
