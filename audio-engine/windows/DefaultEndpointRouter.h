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
    void restore() noexcept;
    [[nodiscard]] bool isActive() const noexcept { return active; }

private:
    std::array<std::wstring, 3> previousEndpointIds;
    bool comInitialised = false;
    bool active = false;
};
}
