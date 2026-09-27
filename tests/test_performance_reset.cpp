#include "PerformanceReset.h"
#include <array>
#include <iostream>
#include <vector>

int main()
{
    int failures = 0;
    const auto require = [&failures](bool condition, const char* message)
    {
        if (! condition) { std::cerr << message << '\n'; ++failures; }
    };
    // Keep checks active in release builds too. This models independent MIDI
    // controllers, not Qin's audio engine; real-plugin sound still needs testing.
    std::array<int, 128> cc {};
    const auto apply = [&cc](const juce::MidiMessage& message)
    {
        if (message.isController()) cc[static_cast<size_t>(message.getControllerNumber())] = message.getControllerValue();
    };
    for (int cycle = 0; cycle < 100; ++cycle)
    {
        fengyin::sendPerformanceReset(false, apply);
        require(cc[11] == 0, "SWAM reset behaviour changed");
        fengyin::sendPerformanceReset(true, apply);
        require(cc[11] == 127 && cc[1] == 0, "Qin reset must release expression mute and clear breath");
        apply(juce::MidiMessage::controllerEvent(1, 1, 96));
        require(cc[11] == 127 && cc[1] == 96, "Qin expression remains muted while blowing");
    }
    std::vector<juce::MidiMessage> messages;
    fengyin::sendPerformanceReset(true, [&](const auto& message) { messages.push_back(message); });
    require(messages.size() == 6, "Reset packet changed unexpectedly");
    require(messages[0].isAllNotesOff() && messages[1].isAllSoundOff(), "Old notes must stop before expression opens");
    require(messages.back().isPitchWheel() && messages.back().getPitchWheelValue() == 8192, "Pitch must return to centre");
    for (const auto& message : messages)
        require(! message.isNoteOn(), "Reset must never play a new note");
    return failures == 0 ? 0 : 1;
}
