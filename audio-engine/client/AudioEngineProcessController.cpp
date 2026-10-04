#include "AudioEngineProcessController.h"

#include <chrono>
#include <thread>
#include <vector>

namespace fengyin::audioengine
{
AudioEngineProcessController::~AudioEngineProcessController()
{
    stop();
}

bool AudioEngineProcessController::start(std::uint32_t bufferFrames,
                                         const juce::String& physicalEndpointId,
                                         bool routeSystemAudio,
                                         juce::String& error)
{
    stop();
#if defined(_WIN32)
    if (bufferFrames != 128 && bufferFrames != 256 && bufferFrames != 512)
    {
        error = "Unsupported FengYin engine buffer";
        return false;
    }

    std::wstring mappingError;
    if (! statusMapping.createOrOpen(instrumentRegionName, mappingError))
    {
        error = juce::String(mappingError.c_str());
        return false;
    }

    if (auto existing = OpenMutexW(SYNCHRONIZE, FALSE, engineSingletonName))
    {
        CloseHandle(existing);
        return waitUntilRunning(3000, error);
    }

    // There is no live engine. Discard state left by an unclean termination
    // before launching the new owner of this protocol generation.
    initialiseRegion(*statusMapping.get());
    const auto executable = juce::File::getSpecialLocation(juce::File::currentExecutableFile)
                                .getSiblingFile("FengYinAudioEngine.exe");
    if (! executable.existsAsFile())
    {
        error = juce::String::fromUTF8("缺少 FengYinAudioEngine.exe");
        statusMapping.close();
        return false;
    }

    auto command = std::wstring(L"\"")
                 + executable.getFullPathName().toWideCharPointer()
                 + L"\" --buffer " + std::to_wstring(bufferFrames);
    if (physicalEndpointId.isNotEmpty())
        command += L" --physical-endpoint \"" + std::wstring(physicalEndpointId.toWideCharPointer()) + L"\"";
    if (routeSystemAudio)
        command += L" --route-system-audio";
    std::vector<wchar_t> mutableCommand(command.begin(), command.end());
    mutableCommand.push_back(L'\0');
    STARTUPINFOW startup {};
    startup.cb = sizeof(startup);
    if (! CreateProcessW(nullptr, mutableCommand.data(), nullptr, nullptr, FALSE,
                         CREATE_NO_WINDOW | CREATE_UNICODE_ENVIRONMENT,
                         nullptr, executable.getParentDirectory().getFullPathName().toWideCharPointer(),
                         &startup, &process))
    {
        error = juce::String::fromUTF8("无法启动风吟音频引擎，Windows 错误：")
              + juce::String(static_cast<int>(GetLastError()));
        statusMapping.close();
        return false;
    }
    CloseHandle(process.hThread);
    process.hThread = nullptr;
    ownsProcess = true;
    if (! waitUntilRunning(5000, error))
    {
        stop();
        return false;
    }
    return true;
#else
    (void) bufferFrames;
    (void) physicalEndpointId;
    (void) routeSystemAudio;
    error = "FengYin audio engine is only available on Windows";
    return false;
#endif
}

bool AudioEngineProcessController::waitUntilRunning(int timeoutMilliseconds,
                                                    juce::String& error) noexcept
{
#if defined(_WIN32)
    const auto deadline = std::chrono::steady_clock::now()
                        + std::chrono::milliseconds(timeoutMilliseconds);
    while (std::chrono::steady_clock::now() < deadline)
    {
        if (statusMapping.get() != nullptr
            && static_cast<StreamState>(statusMapping.get()->state.load(std::memory_order_acquire))
                == StreamState::running
            && statusMapping.get()->activePeriodFrames.load(std::memory_order_acquire) > 0)
            return true;
        if (process.hProcess != nullptr && WaitForSingleObject(process.hProcess, 0) == WAIT_OBJECT_0)
        {
            DWORD exitCode = 0;
            GetExitCodeProcess(process.hProcess, &exitCode);
            error = juce::String::fromUTF8("风吟音频引擎启动失败，错误代码：")
                  + juce::String(static_cast<int>(exitCode));
            return false;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    error = juce::String::fromUTF8("风吟音频引擎启动超时");
    return false;
#else
    (void) timeoutMilliseconds;
    error = "FengYin audio engine is only available on Windows";
    return false;
#endif
}

void AudioEngineProcessController::stop() noexcept
{
#if defined(_WIN32)
    if (ownsProcess && process.hProcess != nullptr)
    {
        if (auto stopEvent = OpenEventW(EVENT_MODIFY_STATE, FALSE, engineStopEventName))
        {
            SetEvent(stopEvent);
            CloseHandle(stopEvent);
        }
        if (WaitForSingleObject(process.hProcess, 2000) == WAIT_TIMEOUT)
            TerminateProcess(process.hProcess, 10);
        CloseHandle(process.hProcess);
    }
    process = {};
#endif
    ownsProcess = false;
    statusMapping.close();
}

bool AudioEngineProcessController::isRunning() const noexcept
{
    if (statusMapping.get() == nullptr) return false;
    const auto state = static_cast<StreamState>(
        statusMapping.get()->state.load(std::memory_order_acquire));
    return state == StreamState::starting || state == StreamState::running
        || state == StreamState::recovering;
}

std::uint32_t AudioEngineProcessController::actualBufferFrames() const noexcept
{
    return statusMapping.get() != nullptr
        ? statusMapping.get()->activePeriodFrames.load(std::memory_order_acquire) : 0;
}
}
