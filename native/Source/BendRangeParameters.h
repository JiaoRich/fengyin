#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <cmath>
#include <optional>

namespace fengyin
{
struct BendRangeParameters
{
    enum Role { none, both, up, down };
    static Role role(juce::String name)
    {
        name = name.toLowerCase().removeCharacters(" ._-/()[]");
        if (name == "pitchbendrange" || name == "bendrange" || name == "pbrange") return both;
        for (const auto* stem : { "pitchbend", "pitchbendrange", "bendrange", "pbrange" })
        {
            if (name == juce::String(stem) + "up") return up;
            if (name == juce::String(stem) + "down") return down;
        }
        return none;
    }
    static bool displays(juce::String text, int semitones)
    {
        text = text.trim().toLowerCase().replace("semitones", "").replace("semitone", "")
            .replace("半音", "").replace("st", "").trim();
        if (text.isEmpty() || text.containsOnly("+-.")) return false;
        if (! text.containsOnly("0123456789+-.")) return false;
        return std::abs(std::abs(text.getDoubleValue()) - semitones) < 0.001;
    }
    static std::optional<float> valueFor(juce::AudioProcessorParameter& parameter, int semitones)
    {
        if (semitones < 1 || semitones > 4) return {};
        // A generic wrapper that only prints normalised 0..1 is not evidence
        // of a semitone scale, even though its endpoint happens to read "1".
        const auto upperText = parameter.getText(1.0f, 128).trim();
        const auto endpointMaximum = juce::jmax(std::abs(upperText.getDoubleValue()),
            std::abs(parameter.getText(0.0f, 128).getDoubleValue()));
        if (endpointMaximum <= 1.0 && ! upperText.containsIgnoreCase("semi")
            && ! upperText.contains("半音")) return {};
        for (const auto sign : { 1, -1 })
        {
            const auto value = parameter.getValueForText(juce::String(sign * semitones));
            if (std::isfinite(value) && value >= 0 && value <= 1
                && displays(parameter.getText(value, 128), semitones)) return value;
        }
        // Some VST3 wrappers don't implement text-to-value. Use only values
        // the plugin itself describes in semitones; never guess its scale.
        const auto steps = juce::jlimit(2, 4097, parameter.getNumSteps());
        for (int i = 0; i < steps; ++i)
        {
            const auto value = static_cast<float>(i) / static_cast<float>(steps - 1);
            if (displays(parameter.getText(value, 128), semitones)) return value;
        }
        return {};
    }
};
}
