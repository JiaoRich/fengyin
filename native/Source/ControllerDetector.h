#pragma once

#include <algorithm>
#include <array>
#include <cstddef>

namespace fengyin
{
class ControllerDetector
{
public:
    void reset() noexcept
    {
        minimum.fill(127);
        maximum.fill(0);
        changes.fill(0);
        last.fill(-1);
    }

    ControllerDetector() { reset(); }

    void observe(int controller, int value) noexcept
    {
        if (controller < 0 || controller > 127)
            return;
        value = std::clamp(value, 0, 127);
        const auto index = static_cast<std::size_t>(controller);
        minimum[index] = std::min(minimum[index], value);
        maximum[index] = std::max(maximum[index], value);
        if (last[index] != value)
            ++changes[index];
        last[index] = value;
    }

    [[nodiscard]] int bestContinuousController() const noexcept
    {
        int best = -1;
        int bestScore = 0;
        for (int controller = 0; controller < 128; ++controller)
        {
            if (controller == 64 || changes[static_cast<std::size_t>(controller)] < 4)
                continue;
            const auto range = maximum[static_cast<std::size_t>(controller)]
                             - minimum[static_cast<std::size_t>(controller)];
            const auto score = range * 8 + std::min(changes[static_cast<std::size_t>(controller)], 127);
            if (range >= 12 && score > bestScore)
            {
                bestScore = score;
                best = controller;
            }
        }
        return best;
    }

private:
    std::array<int, 128> minimum {};
    std::array<int, 128> maximum {};
    std::array<int, 128> changes {};
    std::array<int, 128> last {};
};
}

