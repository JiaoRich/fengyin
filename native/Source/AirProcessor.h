#pragma once

#include <algorithm>
#include <array>
#include <cmath>

namespace fengyin
{
// Parallel, stereo-linked high-frequency detail enhancement. This is not a
// Fresh Air emulation. No synthetic noise, lookahead, allocation or block delay.
class AirProcessor
{
public:
    void prepare(double sampleRate) noexcept
    {
        const auto rate = std::isfinite(sampleRate) && sampleRate >= 8000.0 ? sampleRate : 48000.0;
        // Bilinear one-pole high-pass filters; frequencies stay below Nyquist.
        for (std::size_t band = 0; band < 2; ++band)
        {
            const auto frequency = std::min(band == 0 ? 3500.0 : 9000.0, rate * 0.4);
            const auto k = std::tan(3.141592653589793 * frequency / rate);
            feed[band] = static_cast<float>(1.0 / (1.0 + k));
            feedback[band] = static_cast<float>((1.0 - k) / (1.0 + k));
        }
        ramp = static_cast<float>(1.0 - std::exp(-1.0 / (0.015 * rate)));
        attack = static_cast<float>(1.0 - std::exp(-1.0 / (0.003 * rate)));
        release = static_cast<float>(1.0 - std::exp(-1.0 / (0.100 * rate)));
        reset();
    }

    void reset() noexcept { previousInput = {}; previousHigh = {}; envelope = {}; amount = 0.0f; }

    void process(float* const* outputs, int channels, int samples, float requested) noexcept
    {
        if (outputs == nullptr || channels <= 0 || samples <= 0) return;
        const auto target = std::isfinite(requested) ? std::clamp(requested, 0.0f, 1.0f) : 0.0f;
        if (target == 0.0f && amount == 0.0f) { reset(); return; }
        const auto lanes = std::min(channels, 2);
        for (int sample = 0; sample < samples; ++sample)
        {
            amount += (target - amount) * ramp;
            if (std::abs(target - amount) < 0.000001f) amount = target;
            std::array<std::array<float, 2>, 2> high {};
            for (std::size_t band = 0; band < 2; ++band)
            {
                float peak = 0.0f;
                for (int lane = 0; lane < lanes; ++lane)
                {
                    if (outputs[lane] == nullptr) continue;
                    const auto index = static_cast<std::size_t>(lane);
                    const auto input = outputs[lane][sample];
                    auto value = feed[band] * (input - previousInput[band][index])
                               + feedback[band] * previousHigh[band][index];
                    if (std::abs(value) < 1.0e-20f) value = 0.0f;
                    previousInput[band][index] = input;
                    previousHigh[band][index] = value;
                    high[band][index] = value;
                    peak = std::max(peak, std::abs(value));
                }
                envelope[band] += (peak - envelope[band]) * (peak > envelope[band] ? attack : release);
                if (envelope[band] < 1.0e-20f) envelope[band] = 0.0f;
            }
            // Less boost for already prominent HF transients; linked detection
            // preserves stereo balance. No automatic broadband gain compensation.
            const auto midGain = amount * 0.65f / (1.0f + 6.0f * envelope[0]);
            const auto highGain = amount * 1.15f / (1.0f + 8.0f * envelope[1]);
            for (int lane = 0; lane < lanes; ++lane)
                if (outputs[lane] != nullptr && amount != 0.0f)
                    outputs[lane][sample] += midGain * high[0][static_cast<std::size_t>(lane)]
                                          + highGain * high[1][static_cast<std::size_t>(lane)];
        }
    }

private:
    std::array<float, 2> feed { 0.81f, 0.60f }, feedback { 0.62f, 0.20f }, envelope {};
    std::array<std::array<float, 2>, 2> previousInput {}, previousHigh {};
    float ramp = 0.001388f, attack = 0.00692f, release = 0.000208f, amount = 0.0f;
};
}
