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

// One transition, one restart per held musical note. Initialise controllers
// before the destination NoteOn; never briefly restart the outgoing slot.
template <typename Held, typename Send, typename Velocity>
void changeKongChannel(int previous, int destination, int breath, int bend,
                       Held held, Send send, Velocity velocity, int modulation = -1)
{
    if (previous == destination) return;
    for (int note = 0; note < 128; ++note)
        if (held(note)) send(juce::MidiMessage::noteOff(previous, note));
    send(kongBreathExpression(destination, breath));
    if(modulation>=0) send(juce::MidiMessage::controllerEvent(destination,1,juce::jlimit(0,127,modulation)));
    send(juce::MidiMessage::pitchWheel(destination, juce::jlimit(0, 16383, bend)));
    for (int note = 0; note < 128; ++note)
        if (held(note)) send(juce::MidiMessage::noteOn(destination, note, velocity(note)));
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
