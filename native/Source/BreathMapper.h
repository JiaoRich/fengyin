#pragma once

#include <algorithm>
#include <cmath>

namespace fengyin
{
class BreathMapper
{
public:
    struct Settings
    {
        float threshold = 0.025f;
        float curve = 1.0f;
        float smoothing = 0.22f;
    };

    void setSettings(Settings next) noexcept
    {
        settings.threshold = std::clamp(next.threshold, 0.0f, 0.95f);
        settings.curve = std::clamp(next.curve, 0.25f, 4.0f);
        settings.smoothing = std::clamp(next.smoothing, 0.01f, 1.0f);
    }

    [[nodiscard]] Settings getSettings() const noexcept { return settings; }

    float processMidiValue(int midiValue) noexcept
    {
        const auto input = static_cast<float>(std::clamp(midiValue, 0, 127)) / 127.0f;
        const auto normalised = input <= settings.threshold
            ? 0.0f
            : (input - settings.threshold) / (1.0f - settings.threshold);
        const auto shaped = std::pow(std::clamp(normalised, 0.0f, 1.0f), settings.curve);
        current += (shaped - current) * settings.smoothing;
        if (current < 0.0001f)
            current = 0.0f;
        return current;
    }

    void reset() noexcept { current = 0.0f; }
    [[nodiscard]] float getCurrentValue() const noexcept { return current; }

private:
    Settings settings;
    float current = 0.0f;
};
}
