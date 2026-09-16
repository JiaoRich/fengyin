#pragma once

namespace fengyin::PitchKey
{
[[nodiscard]] constexpr int normalise(int value) noexcept
{
    value %= 12;
    return value < 0 ? value + 12 : value;
}

// 返回离目标最近的升降半音数。相差增四/减五度时统一向上 6 个半音。
[[nodiscard]] constexpr int transposeFromTo(int sourcePitchClass, int targetPitchClass) noexcept
{
    auto difference = normalise(targetPitchClass) - normalise(sourcePitchClass);
    if (difference > 6) difference -= 12;
    if (difference < -6) difference += 12;
    return difference;
}
}
