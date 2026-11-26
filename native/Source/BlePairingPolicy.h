#pragma once
#include <juce_core/juce_core.h>

namespace fengyin
{
// Limit the new ceremony to the model for which an ATT encryption requirement
// and failed basic PairAsync were actually observed. Other controllers retain
// their original pairing path; an existing bond never enters this ceremony.
inline bool useYdsCustomPairing(const juce::String& name, const juce::String& action, bool paired)
{
    const auto model = name.toLowerCase().removeCharacters(" -_");
    return !paired && (action == "connect" || action == "repair")
        && (model == "yds150" || model == "yds150midi");
}
}
