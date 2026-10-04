#pragma once

#include "FengYinAudioProtocol.h"

#if defined(_WIN32)
#include <windows.h>
#endif

#include <atomic>
#include <string>
#include <thread>

namespace fengyin::audioengine
{
class WasapiLoopbackInput
{
public:
    WasapiLoopbackInput() = default;
    ~WasapiLoopbackInput();
    WasapiLoopbackInput(const WasapiLoopbackInput&) = delete;
    WasapiLoopbackInput& operator=(const WasapiLoopbackInput&) = delete;

    bool start(SharedAudioRegion& destination, std::wstring& error);
    void stop() noexcept;
    [[nodiscard]] bool isRunning() const noexcept { return running.load(std::memory_order_acquire); }

private:
    void run(SharedAudioRegion* destination) noexcept;
    std::thread worker;
    std::atomic<bool> stopRequested { false };
    std::atomic<bool> running { false };
#if defined(_WIN32)
    HANDLE startedEvent = nullptr;
#endif
    std::wstring startError;
};
}
