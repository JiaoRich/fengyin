#include "PerformanceReset.h"
#include "KongPerformancePolicy.h"
#include "TechniqueSignal.h"
#include "BreathResponse.h"
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
    require(fengyin::performanceBreath(0) == 0, "No breath must remain silent");
    require(fengyin::performanceBreath(1) == 1, "Full breath must retain full scale");
    constexpr float inputs[] = {.1f, .3f, .5f, .7f, .9f};
    constexpr float outputs[] = {.1f, .3f, .5f, .7f, .9f};
    for (int k = 0; k < 5; ++k)
    {
        require(std::abs(fengyin::performanceBreath(inputs[k]) - outputs[k]) < .00001f,
                "Curve must pass through every user-approved knot");
        constexpr float h = .0001f;
        const float left = (fengyin::performanceBreath(inputs[k]) - fengyin::performanceBreath(inputs[k]-h)) / h;
        const float right = (fengyin::performanceBreath(inputs[k]+h) - fengyin::performanceBreath(inputs[k])) / h;
        require(std::abs(left-right) < .01f, "Curve slope must be continuous at knots");
    }
    for (int cc = 0; cc <= 127; ++cc)
        require(fengyin::performanceBreath(cc / 127.0f) == cc / 127.0f, "Every incoming CC must pass through unchanged");
    for (int i = 0; i < 127; ++i)
        require(fengyin::performanceBreath(i / 127.0f) < fengyin::performanceBreath((i+1) / 127.0f),
                "Breath response must remain strictly increasing");
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
        require(std::abs(fengyin::performanceVelocity(true, input) - input) < 0.0001f,
                "Qin velocity must remain unchanged");
        require(std::abs(fengyin::performanceVelocity(false, input) - input) < 0.0001f,
                "SWAM velocity must remain unchanged");
    }
    fengyin::changeKongArticulation(1, 26, [](int) { return false; },
        [&](const auto& message) { messages.push_back(message); }, [](int) { return 0.7f; });
    require(messages.empty(), "Idle bite must not emit keyswitch notes");
    fengyin::changeKongArticulation(1, 26, [](int n) { return n == 60; },
        [&](const auto& message) { messages.push_back(message); }, [](int) { return 0.7f; });
    require(messages.size() == 4 && messages[0].isNoteOff() && messages[0].getNoteNumber() == 60
        && messages[1].isNoteOn() && messages[1].getNoteNumber() == 26
        && messages[2].isNoteOff() && messages[2].getNoteNumber() == 26
        && messages[3].isNoteOn() && messages[3].getNoteNumber() == 60,
        "Articulation must select between old note off and same-pitch restart");
    require(std::abs(messages[3].getFloatVelocity()-0.7f)<0.01f,"Restart must preserve original velocity");
    messages.clear();
    for (int cycle = 0; cycle < 100; ++cycle)
    {
        for (int key : {26, 24})
            fengyin::changeKongArticulation(1, key, [](int n) { return n == 60; },
                [&](const auto& message) { messages.push_back(message); }, [](int) { return 0.7f; });
    }
    require(messages.size() == 800, "Repeated press/release must restart held voice exactly once");
    for (const auto& message : messages)
        require(message.isNoteOnOrOff() && (message.getNoteNumber() == 24 || message.getNoteNumber() == 26 || message.getNoteNumber() == 60),
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
