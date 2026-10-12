#pragma once
#include <juce_audio_basics/juce_audio_basics.h>

namespace fengyin
{
inline juce::MidiMessage kongBreathExpression(int channel, int value)
{
    return juce::MidiMessage::controllerEvent(channel, 11, juce::jlimit(0, 127, value));
}
// Preserve the player's velocity; sample dynamics belong to the sound source.
inline float performanceVelocity(bool, float input) noexcept
{
    return juce::jlimit(0.0f, 1.0f, input);
}

template <typename Held, typename Send>
void changeKongArticulation(int channel, int key, Held held, Send send)
{
    bool playing = false;
    for (int note = 0; note < 128; ++note)
        if (held(note)) { playing = true; break; }
    if (!playing) return; // Idle controls must not create sampler voices.
    // Send only the articulation command. Releasing/retriggering the musical
    // note here restarts the sample attack and introduces an audible gap.
    send(juce::MidiMessage::noteOn(channel, key, static_cast<juce::uint8>(100)));
    send(juce::MidiMessage::noteOff(channel, key));
}
}
