#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "AudioCallbackTrace.h"
#include <functional>

namespace fengyin
{
class TechniqueProcessingGraph final : public juce::AudioProcessorGraph
{
public:
    TechniqueProcessingGraph() { filteredMidi.ensureSize(256 * 1024); }
    std::function<void(juce::MidiBuffer&)> preparePerformanceMidi;
    std::function<void()> applyTechniqueValues;
    std::function<bool(const juce::MidiMessage&)> suppressTechniqueMidi;
    std::function<void(juce::MidiBuffer&,int,double)> transformPerformanceMidi;
    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& messages) override
    {
        audioTrace::Scope techniqueTime(audioTrace::Event::techniques);
        if (preparePerformanceMidi) preparePerformanceMidi(messages);
        if (applyTechniqueValues) applyTechniqueValues();
        if (suppressTechniqueMidi)
        {
            filteredMidi.clear();
            for (const auto metadata : messages)
                if (!suppressTechniqueMidi(metadata.getMessage()))
                    filteredMidi.addEvent(metadata.data,metadata.numBytes,metadata.samplePosition);
            messages.swapWith(filteredMidi);
        }
        if (transformPerformanceMidi) transformPerformanceMidi(messages,buffer.getNumSamples(),getSampleRate());
        techniqueTime.finish();
        juce::AudioProcessorGraph::processBlock(buffer, messages);
    }
private:
    juce::MidiBuffer filteredMidi;
};
}
