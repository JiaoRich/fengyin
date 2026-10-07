#pragma once
#include <algorithm>

namespace fengyin
{
// Digitised reference at 10% intervals. Monotone cubic Hermite interpolation
// preserves the measured shape without overshoot; zero remains silent.
inline float performanceBreath(float input) noexcept
{
    const auto x = std::clamp(input, 0.0f, 1.0f);
    constexpr float y[] = {0, .113f, .265f, .431f, .591f, .730f,
                           .837f, .914f, .964f, .985f, 1};
    if (x >= 1) return 1;
    const int i = static_cast<int>(x * 10);
    const float t = x * 10 - i;
    const auto slope = [&](int k)
    {
        if (k == 0) return y[1] - y[0];
        if (k == 10) return y[10] - y[9];
        const float a = y[k] - y[k-1], b = y[k+1] - y[k];
        return 2 * a * b / (a + b);
    };
    return (2*t*t*t-3*t*t+1)*y[i] + (t*t*t-2*t*t+t)*slope(i)
         + (-2*t*t*t+3*t*t)*y[i+1] + (t*t*t-t*t)*slope(i+1);
}
}
