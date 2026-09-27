#include "BendRangeParameters.h"
#include <cassert>

int main()
{
    using B = fengyin::BendRangeParameters;
    assert(B::role("Pitch Bend Up") == B::up);
    assert(B::role("pitchBendDown") == B::down);
    assert(B::role("Pitch Bend MIDI CC") == B::none);
    assert(! B::displays("30 cents", 3));
    assert(! B::displays("0..3", 3));
    assert(B::displays("-3 semitones", 3));
    juce::AudioParameterInt up({"pitchBendUp", 1}, "Pitch Bend Up", 0, 24, 2);
    juce::AudioParameterInt down({"pitchBendDown", 1}, "Pitch Bend Down", -24, 0, -2);
    for (int semitones = 1; semitones <= 4; ++semitones)
    {
        const auto a = B::valueFor(up, semitones), b = B::valueFor(down, semitones);
        assert(a && b);
        assert(B::displays(static_cast<juce::AudioProcessorParameter&>(up).getText(*a, 128), semitones));
        assert(B::displays(static_cast<juce::AudioProcessorParameter&>(down).getText(*b, 128), semitones));
    }
    juce::AudioParameterFloat normalised({"pitchBendRange", 1}, "Pitch Bend Range", 0.0f, 1.0f, 0.5f);
    assert(! B::valueFor(normalised, 1));
    assert(! B::valueFor(up, 0));
    assert(! B::valueFor(up, 5));
    juce::StringArray upChoices, downChoices;
    for (int n = 0; n <= 12; ++n) { upChoices.add(juce::String(12 - n)); downChoices.add(juce::String(-n)); }
    juce::AudioParameterChoice qinUp({"PitchbendUp", 1}, "PitchbendUp", upChoices, 10);
    juce::AudioParameterChoice qinDown({"PitchbendDown", 1}, "PitchbendDown", downChoices, 2);
    for (int semitones = 1; semitones <= 4; ++semitones)
    {
        const auto a = B::valueFor(qinUp, semitones), b = B::valueFor(qinDown, semitones);
        assert(a && b);
        assert(std::abs(*a - (12.0f - static_cast<float>(semitones)) / 12.0f) < 0.001f);
        assert(std::abs(*b - static_cast<float>(semitones) / 12.0f) < 0.001f);
    }
    return 0;
}
