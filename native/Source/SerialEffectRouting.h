#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <vector>

namespace fengyin
{
inline bool connectSerialEffects(juce::AudioProcessorGraph& graph,
                                juce::AudioProcessorGraph::Node::Ptr source,
                                const std::vector<juce::AudioProcessorGraph::Node::Ptr>& effects,
                                juce::AudioProcessorGraph::Node::Ptr destination)
{
    for (const auto& effect : effects)
    {
        if (effect->isBypassed()) continue;
        const auto outputs = source->getProcessor()->getTotalNumOutputChannels();
        const auto inputs = effect->getProcessor()->getTotalNumInputChannels();
        if (outputs < 1 || inputs < 1 || effect->getProcessor()->getTotalNumOutputChannels() < 1) return false;
        for (int channel = 0; channel < juce::jmin(2, inputs); ++channel)
            if (! graph.addConnection({{source->nodeID, juce::jmin(channel, outputs - 1)}, {effect->nodeID, channel}})) return false;
        source = effect;
    }
    const auto outputs = source->getProcessor()->getTotalNumOutputChannels();
    if (outputs < 1) return false;
    for (int channel = 0; channel < 2; ++channel)
        if (! graph.addConnection({{source->nodeID, juce::jmin(channel, outputs - 1)}, {destination->nodeID, channel}})) return false;
    return true;
}
}
