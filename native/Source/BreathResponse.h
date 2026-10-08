#pragma once
#include <algorithm>

namespace fengyin
{
// User-approved knots. Nonuniform monotone cubic Hermite interpolation
// passes through every knot with continuous slope and no overshoot.
inline float performanceBreath(float input) noexcept
{
    const auto x = std::clamp(input, 0.0f, 1.0f);
    constexpr float knots[] = {0, .10f, .30f, .50f, .70f, .90f, 1};
    constexpr float y[] = {0, .12f, .36f, .60f, .85f, .95f, 1};
    if (x >= 1) return 1;
    int i = 0;
    while (i < 5 && x > knots[i + 1]) ++i;
    const float width = knots[i + 1] - knots[i];
    const float t = (x - knots[i]) / width;
    const auto slope = [&](int k)
    {
        if (k == 0) return (y[1] - y[0]) / (knots[1] - knots[0]);
        if (k == 6) return (y[6] - y[5]) / (knots[6] - knots[5]);
        const float left = knots[k] - knots[k-1], right = knots[k+1] - knots[k];
        const float a = (y[k] - y[k-1]) / left, b = (y[k+1] - y[k]) / right;
        const float w1 = 2 * right + left, w2 = right + 2 * left;
        return (w1 + w2) / (w1 / a + w2 / b);
    };
    return (2*t*t*t-3*t*t+1)*y[i] + (t*t*t-2*t*t+t)*width*slope(i)
         + (-2*t*t*t+3*t*t)*y[i+1] + (t*t*t-t*t)*width*slope(i+1);
}
}
