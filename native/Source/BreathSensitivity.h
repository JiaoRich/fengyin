#pragma once
#include <algorithm>
#include <array>
#include <cmath>

namespace fengyin
{
// Input-device response only. Never write this into a plug-in/preset curve.
// Monotone C1 Hermite curves keep the quiet toe unchanged and preserve both
// endpoints. The easy response is spread over a wider interval than the first
// mock-up to reduce the maximum local slope and 7-bit controller steps.
struct BreathSensitivity
{
    static constexpr int valid(int strength) noexcept { return std::clamp(strength, -100, 100); }
    static float apply(float raw, int strength) noexcept
    {
        const auto x = std::isfinite(raw) ? std::clamp(raw, 0.0f, 1.0f) : 0.0f;
        strength = valid(strength);
        if (strength == 0 || x <= 0.1f || x == 1.0f) return x;
        constexpr std::array<float, 5> xs { .1f, .35f, .5f, .75f, 1.0f };
        constexpr std::array<float, 5> easy { .1f, .60f, .82f, .95f, 1.0f };
        constexpr std::array<float, 5> hard { .1f, .22f, .35f, .63f, 1.0f };
        const auto& ys = strength > 0 ? easy : hard;
        std::array<float, 4> h {}, d {};
        std::array<float, 5> m { 1.0f };
        for (size_t i = 0; i < 4; ++i) { h[i] = xs[i+1]-xs[i]; d[i] = (ys[i+1]-ys[i])/h[i]; }
        for (size_t i = 1; i < 4; ++i)
        {
            const auto w1 = 2*h[i]+h[i-1], w2 = h[i]+2*h[i-1];
            m[i] = (w1+w2)/(w1/d[i-1]+w2/d[i]);
        }
        m[4] = std::clamp(((2*h[3]+h[2])*d[3]-h[3]*d[2])/(h[3]+h[2]), 0.0f, 3*d[3]);
        size_t i = 0; while (i < 3 && x > xs[i+1]) ++i;
        const auto t = (x-xs[i])/h[i], t2 = t*t, t3 = t2*t;
        const auto y = (2*t3-3*t2+1)*ys[i]+(t3-2*t2+t)*h[i]*m[i]
                     +(-2*t3+3*t2)*ys[i+1]+(t3-t2)*h[i]*m[i+1];
        return std::clamp(x+(y-x)*(static_cast<float>(std::abs(strength))/100.0f), 0.0f, 1.0f);
    }
};
}
