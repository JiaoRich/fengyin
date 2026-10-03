#pragma once
#include "ToneParameterValue.h"
#include <cmath>

namespace fengyin
{
// Only undo values explicitly owned by the previous preset and absent from the
// next one. Never reset the whole plugin on first load or a repeated request.
inline juce::Array<ToneParameterValue> retiredToneParameters(
    const juce::Array<ToneParameterValue>& previous,
    const juce::Array<ToneParameterValue>& next,
    const juce::Array<ToneParameterValue>& baseline)
{
    juce::Array<ToneParameterValue> result;
    for (const auto& old : previous)
    {
        bool retained = false;
        for (const auto& value : next)
            if (value.identifier == old.identifier) { retained = true; break; }
        if (retained) continue;
        for (const auto& value : baseline)
            if (value.identifier == old.identifier) { result.add(value); break; }
    }
    return result;
}
inline bool toneValueNeedsWrite(float current, float desired)
{
    return std::isfinite(desired) && desired >= 0.0f && desired <= 1.0f
        && (!std::isfinite(current) || std::abs(current - desired) > 0.000001f);
}
}
