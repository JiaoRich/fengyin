#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace fengyin
{
struct SwamExpressionCurve
{
    // Preserve existing native curves. Missing curves receive an identity default.
    static constexpr double inputMinimum = 0.0;
    static constexpr double inputMaximum = 127.0;
    static constexpr double outputMinimum = 0.0;
    static constexpr double outputMaximum = 127.0;
    static constexpr double shape = 0.0;
    static constexpr double symmetry = 0.50;

    struct Result
    {
        bool found = false;
        bool changed = false;
        int controller = -1;
        struct Route { juce::String parameter; int controller; int channel; };
        std::vector<Route> routes;
    };

    // Preserve all existing curves and mappings; initialise only an absent curve.
    static Result applyToXml(juce::XmlElement& root);

    // Supports plain/binary XML and read-only wrapped VST3 component mappings.
    static Result applyToState(juce::MemoryBlock& state);
};
}
