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

template <typename Held, typename Send, typename Velocity>
void changeKongArticulation(int channel, int key, Held held, Send send, Velocity velocity)
{
    bool playing = false;
    for (int note = 0; note < 128; ++note)
        if (held(note)) { playing = true; break; }
    if (!playing) return; // Idle controls must not create sampler voices.
    // Keyswitches select future sampler voices, not an already playing voice.
    // End old voices before selecting, then restart at the original velocity.
    for (int note = 0; note < 128; ++note)
        if (held(note)) send(juce::MidiMessage::noteOff(channel, note));
    send(juce::MidiMessage::noteOn(channel, key, static_cast<juce::uint8>(100)));
    send(juce::MidiMessage::noteOff(channel, key));
    for (int note = 0; note < 128; ++note)
        if (held(note)) send(juce::MidiMessage::noteOn(channel, note, velocity(note)));
}
}
