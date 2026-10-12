#pragma once
#include <algorithm>

namespace fengyin
{
// Compatibility helper: shaping belongs to each instrument's saved curve.
inline float performanceBreath(float input) noexcept
{
    return std::clamp(input, 0.0f, 1.0f);
}
}
