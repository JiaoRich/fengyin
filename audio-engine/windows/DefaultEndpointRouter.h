#pragma once

#include <array>
#include <string>

namespace fengyin::audioengine
{
class DefaultEndpointRouter
{
public:
    DefaultEndpointRouter() = default;
    ~DefaultEndpointRouter();
    DefaultEndpointRouter(const DefaultEndpointRouter&) = delete;
    DefaultEndpointRouter& operator=(const DefaultEndpointRouter&) = delete;

    bool routeSystemAudioToFengYin(std::wstring& physicalEndpointId,
                                  std::wstring& error) noexcept;
    // Windows may make a newly inserted USB headset/speaker the default even
    // while the bridge is active. Capture that physical choice, update the
    // crash-recovery journal, then immediately restore the virtual route.
    bool pollPhysicalDefaultChange(std::wstring& physicalEndpointId,
                                   std::wstring& error) noexcept;
    void restore() noexcept;
    static bool restorePendingRoute(std::wstring& error) noexcept;
    [[nodiscard]] bool isActive() const noexcept { return active; }

private:
    std::array<std::wstring, 3> previousEndpointIds;
    bool comInitialised = false;
    bool active = false;
};
}
