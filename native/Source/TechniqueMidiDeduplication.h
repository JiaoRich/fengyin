#pragma once
#include <atomic>
#include <cstddef>

namespace fengyin
{
template <typename Routes, typename Cache>
void invalidateTechniqueController(const Routes& routes, Cache& cache, int route) noexcept
{
    for (std::size_t index = 0; index < routes.size(); ++index)
        if (routes[index].load(std::memory_order_relaxed) == route)
            cache[index].store(0, std::memory_order_relaxed);
}

// Zero means unknown; include route and value so a remap never hides its first event.
template <typename Send>
bool sendTechniqueController(std::atomic<int>& previous, int route, int value,
                             bool uniqueRoute, Send&& send)
{
    const int token = (route << 8) | (value + 1);
    if (uniqueRoute && previous.load(std::memory_order_relaxed) == token) return false;
    if (!send()) return false; // A full queue must not poison the retry cache.
    previous.store(uniqueRoute ? token : 0, std::memory_order_relaxed);
    return true;
}
}
