#pragma once
#include <juce_audio_basics/juce_audio_basics.h>

namespace fengyin
{
inline juce::MidiMessage kongBreathExpression(int channel, int value)
{
    return juce::MidiMessage::controllerEvent(channel, 11, juce::jlimit(0, 127, value));
}
// Wind attacks arrive before breath settles. Do not latch that transient low
// velocity into Qin's sample gain; dynamics continue through its breath CC.
inline float performanceVelocity(bool kong, float input) noexcept
{
    return kong ? 96.0f / 127.0f : juce::jlimit(0.0f, 1.0f, input);
}

template <typename Held, typename Velocity, typename Send>
void changeKongArticulation(int channel, int key, Held held, Velocity velocity, Send send)
{
    bool playing = false;
    for (int note = 0; note < 128; ++note)
        if (held(note)) { playing = true; send(juce::MidiMessage::noteOff(channel, note)); }
    if (!playing) return; // Idle controls must not create sampler voices.
    send(juce::MidiMessage::noteOn(channel, key, static_cast<juce::uint8>(100)));
    send(juce::MidiMessage::noteOff(channel, key));
    for (int note = 0; note < 128; ++note)
        if (held(note)) send(juce::MidiMessage::noteOn(channel, note, velocity(note)));
}
}
