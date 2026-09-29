#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

namespace fengyin
{
// Do not call AudioProcessorPlayer::setProcessor(nullptr) for editor/state/FX
// operations: releaseResources/prepareToPlay can invalidate a sampler's rack.
class ScopedGraphPause
{
public:
    explicit ScopedGraphPause(juce::AudioProcessor* graph) : processor(graph)
    {
        if (processor)
        {
            const juce::ScopedLock lock(processor->getCallbackLock());
            wasSuspended = processor->isSuspended();
            processor->suspendProcessing(true);
        }
    }
    ~ScopedGraphPause()
    {
        if (processor)
        {
            const juce::ScopedLock lock(processor->getCallbackLock());
            processor->suspendProcessing(wasSuspended);
        }
    }
private:
    juce::AudioProcessor* processor;
    bool wasSuspended = false;
};
}
