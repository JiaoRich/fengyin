#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace fengyin
{
struct SwamExpressionCurve
{
    // The accepted main-breath curve keeps the full input range, starts at
    // silence, gently opens through the low/mid range and caps the hottest
    // output at 116/127 to avoid the harsh top edge. SWAM represents this
    // curve with shape/symmetry rather than arbitrary points.
    static constexpr double inputMinimum = 0.0;
    static constexpr double inputMaximum = 127.0;
    static constexpr double outputMinimum = 0.0;
    static constexpr double outputMaximum = 116.0;
    static constexpr double shape = 0.10;
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
