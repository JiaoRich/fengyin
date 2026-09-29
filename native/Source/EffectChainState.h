#pragma once
#include <juce_core/juce_core.h>
#include <vector>

namespace fengyin
{
struct EffectChainState
{
    juce::String descriptionXml; // Full VST3 identity, version and path; resolve by UID on another PC.
    juce::MemoryBlock state;
    bool bypassed = false;
};
using EffectChainStates = std::vector<EffectChainState>;
}
