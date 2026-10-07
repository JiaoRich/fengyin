#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

namespace fengyin
{
// Clear modulation and release the expression gain during plugin reset.
// Qin's live breath path now drives CC11 (including zero on breath release),
// and re-sends it before each attack. Do not route breath into modulation CC1.
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
