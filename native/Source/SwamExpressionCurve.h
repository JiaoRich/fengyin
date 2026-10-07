#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace fengyin
{
struct SwamExpressionCurve
{
    // The host applies the shared breath response once. Bypass SWAM's local
    // remapping curve to prevent a second nonlinear transformation.
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
    };

    // Updates only the curve belonging to the existing expression mapping.
    // Controller number, channel, message type and every other MIDI mapping
    // remain untouched.
    static Result applyToXml(juce::XmlElement& root);

    // Supports both plain .swam XML and JUCE binary XML plugin states.
    static Result applyToState(juce::MemoryBlock& state);
};
}
