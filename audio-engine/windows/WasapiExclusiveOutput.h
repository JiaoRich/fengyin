#pragma once

#include "AudioEngineCore.h"

#if defined(_WIN32)
#include <windows.h>
#include <audioclient.h>
#include <mmdeviceapi.h>
#include <wrl/client.h>
#endif

#include <array>
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>

namespace fengyin::audioengine
{
class WasapiExclusiveOutput
{
public:
    WasapiExclusiveOutput() = default;
    ~WasapiExclusiveOutput();
    WasapiExclusiveOutput(const WasapiExclusiveOutput&) = delete;
    WasapiExclusiveOutput& operator=(const WasapiExclusiveOutput&) = delete;

    bool start(AudioEngineCore& engine, std::uint32_t requestedFrames,
               const std::wstring& preferredEndpointId, std::wstring& error);
    void stop() noexcept;
    [[nodiscard]] std::uint32_t actualBufferFrames() const noexcept { return activeFrames.load(); }
    [[nodiscard]] bool isRunning() const noexcept { return running.load(); }

private:
    void run(AudioEngineCore* engine, std::uint32_t requestedFrames,
             std::wstring preferredEndpointId) noexcept;
    void reportStarted(bool success, const std::wstring& message, std::uint32_t frames) noexcept;

    std::thread worker;
    std::atomic<bool> stopRequested { false };
    std::atomic<bool> running { false };
    std::atomic<std::uint32_t> activeFrames { 0 };
    std::mutex startMutex;
    std::condition_variable startCondition;
    bool startReported = false;
    bool startSucceeded = false;
    std::wstring startMessage;
#if defined(_WIN32)
    HANDLE instrumentRequest = nullptr;
#endif
};
}
