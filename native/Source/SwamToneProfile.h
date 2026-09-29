#pragma once
#include <vector>
#include "ToneParameterValue.h"

namespace fengyin
{
struct SwamToneTarget
{
    const char* name;
    const char* display;
};
// Values are normalised VST parameters.  This profile intentionally contains
// acoustic/timbre controls only: MIDI mapping, expression, breath, pitch bend
// and performance-technique parameters are never part of a tone style.
struct SwamToneProfile
{
    bool enabled = false;
    float timbre = 0.5f;
    float brightness = 0.5f;
    float formant = 0.5f;
    float reedStiffness = 0.5f;
    float attack = 0.5f;
    float harmonics = 0.5f;
    float keyNoise = 0.15f;
    float resonance = 0.5f;
    float breathNoise = 0.5f;
    std::vector<SwamToneTarget> displayTargets {};
    juce::Array<ToneParameterValue> releaseParameters;
    juce::String releaseModel;

    static SwamToneProfile soprano(int style)
    {
        SwamToneProfile result;
        result.enabled = true;
        const auto pick = [style](const char* natural, const char* silky, const char* stage)
        { return style == 1 ? silky : style == 2 ? stage : natural; };
        result.displayTargets = {
            { "Key Noise", pick("18", "8", "12") },
            { "Harmonic Structure", pick("0.00", "-0.08", "0.10") },
            { "Sub. Harm", pick("12", "18", "8") },
            { "Formant", pick("0.0", "-0.8", "0.7") },
            { "Modal Res. Gain", pick("50", "62", "40") },
            { "Breath Noise", pick("48", "62", "32") },
            { "Timbral Correction", pick("OFF", "ON", "ON") },
            { "Harmonic A Gain", pick("0", "1.2", "1.0") },
            { "Harm. A", pick("2", "2", "3") },
            { "Harmonic B Gain", pick("0", "-0.8", "0.5") },
            { "Harm. B", pick("3", "3", "5") },
            { "Compressor", "5" },
            { "EQ Enabled", pick("OFF", "ON", "ON") },
            { "EQ Low Gain", pick("0", "0.5", "-0.5") },
            { "EQ Mid Gain", pick("0", "-1.5", "1.0") },
            { "EQ Mid Freq", pick("1800", "2500", "1800") },
            { "EQ High Gain", pick("0", "-1.0", "0.5") },
            { "Random Dynamic", pick("5", "8", "3") },
            { "Dynamic Pitch", pick("20", "25", "15") },
            { "Dynamic Harmonic", pick("0.45", "0.30", "0.70") },
            { "Release Time", "10" },
            { "Breathy ppp", "OFF" }
        };
        return result;
    }
};
}
