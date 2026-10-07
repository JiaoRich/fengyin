#include "PerformanceReset.h"
#include "KongPerformancePolicy.h"
#include "TechniqueSignal.h"
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
    require(fengyin::techniquePitchAmount(8192) == 0.0f, "Wheel centre must release technique");
    require(fengyin::techniquePitchAmount(0) == 1.0f, "Negative wheel must reach full technique");
    require(fengyin::techniquePitchAmount(16383) == 1.0f, "Positive wheel must reach full technique");
    require(fengyin::techniquePitchAmount(8191) < 0.001f, "Centre jitter must not trigger technique");
    for (int value = 8192; value < 16383; ++value)
        require(fengyin::techniquePitchAmount(value) <= fengyin::techniquePitchAmount(value + 1),
                "Wheel technique response must be monotonic");
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
        for (int breath : {0, 10, 32, 64, 96, 127, 64, 0})
        {
            apply(fengyin::kongBreathExpression(1, breath));
            require(cc[11] == breath && cc[1] == 0, "Qin breath must drive expression, not modulation");
        }
    }
    std::vector<juce::MidiMessage> messages;
    for (int channel = 1; channel <= 16; ++channel)
    {
        const auto expression = fengyin::kongBreathExpression(channel, 64);
        require(expression.getChannel() == channel && expression.getControllerNumber() == 11
            && expression.getControllerValue() == 64, "Destination slot expression must follow breath");
    }
    for (int velocity : {10, 11, 17, 48, 68, 127})
    {
        const auto input = static_cast<float>(velocity) / 127.0f;
        require(std::abs(fengyin::performanceVelocity(true, input) - 96.0f / 127.0f) < 0.0001f,
                "Qin first and subsequent attacks must have equal velocity");
        require(std::abs(fengyin::performanceVelocity(false, input) - input) < 0.0001f,
                "SWAM velocity must remain unchanged");
    }
    fengyin::changeKongArticulation(1, 26, [](int) { return false; },
        [&](const auto& message) { messages.push_back(message); });
    require(messages.empty(), "Idle bite must not emit keyswitch notes");
    fengyin::changeKongArticulation(1, 26, [](int n) { return n == 60; },
        [&](const auto& message) { messages.push_back(message); });
    require(messages.size() == 2 && messages[0].isNoteOn() && messages[0].getNoteNumber() == 26
        && messages[1].isNoteOff() && messages[1].getNoteNumber() == 26,
        "Articulation must not interrupt or retrigger the musical note");
    messages.clear();
    for (int cycle = 0; cycle < 100; ++cycle)
    {
        for (int key : {26, 24})
            fengyin::changeKongArticulation(1, key, [](int n) { return n == 60; },
                [&](const auto& message) { messages.push_back(message); });
    }
    require(messages.size() == 400, "Repeated bite/release must emit exactly paired keyswitches");
    for (const auto& message : messages)
        require(message.isNoteOnOrOff() && (message.getNoteNumber() == 24 || message.getNoteNumber() == 26),
            "Bite/release must not change pitch, expression or the held musical note");
    messages.clear();
    fengyin::sendPerformanceReset(true, [&](const auto& message) { messages.push_back(message); });
    require(messages.size() == 6, "Reset packet changed unexpectedly");
    require(messages[0].isAllNotesOff() && messages[1].isAllSoundOff(), "Old notes must stop before expression opens");
    require(messages.back().isPitchWheel() && messages.back().getPitchWheelValue() == 8192, "Pitch must return to centre");
    for (const auto& message : messages)
        require(! message.isNoteOn(), "Reset must never play a new note");
    return failures == 0 ? 0 : 1;
}
