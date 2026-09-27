#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

namespace fengyin
{
// Qin uses CC1 for the wind controller and a separate CC11 expression gain.
// Clearing CC11 while subsequently sending only CC1 leaves the instrument muted.
// This is runtime MIDI state, not a reason to rewrite the saved sampler preset.
template <typename Send>
void sendPerformanceReset(bool kong, Send&& send)
{
    send(juce::MidiMessage::allNotesOff(1));
    send(juce::MidiMessage::allSoundOff(1));
    send(juce::MidiMessage::controllerEvent(1, 11, kong ? 127 : 0));
    send(juce::MidiMessage::controllerEvent(1, 2, 0));
    send(juce::MidiMessage::controllerEvent(1, 1, 0));
    send(juce::MidiMessage::pitchWheel(1, 8192));
}
}
