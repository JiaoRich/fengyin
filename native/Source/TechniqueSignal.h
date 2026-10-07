#pragma once
#include <juce_audio_basics/juce_audio_basics.h>

namespace fengyin
{
// Pitch-wheel hardware rests at 8192. For a unipolar technique amount,
// either direction away from centre means increasing intensity, not pitch.
inline float techniquePitchAmount(int value) noexcept
{
    value = juce::jlimit(0, 16383, value);
    return value < 8192 ? static_cast<float>(8192 - value) / 8192.0f
                        : static_cast<float>(value - 8192) / 8191.0f;
}
}
