#pragma once
#include "MasterOutputService.h"
#include <juce_audio_processors/juce_audio_processors.h>

namespace fengyin {
// Keep the original non-reverb tone processing before the external reverb.
// Allocated only during graph preparation; no buffers or locks in processBlock.
class InstrumentToneProcessor final : public juce::AudioProcessor {
public:
    explicit InstrumentToneProcessor(MasterOutputService& service)
        :AudioProcessor(BusesProperties().withInput("In",juce::AudioChannelSet::stereo(),true)
            .withOutput("Out",juce::AudioChannelSet::stereo(),true)), output(service) {}
    const juce::String getName() const override { return "FengYin tone processing"; }
    void prepareToPlay(double,int) override {} void releaseResources() override {}
    void processBlock(juce::AudioBuffer<float>& b,juce::MidiBuffer&) override {
        output.processInstrumentEffects(b.getArrayOfWritePointers(),b.getNumChannels(),b.getNumSamples(),false);
    }
    double getTailLengthSeconds()const override{return 0;} bool acceptsMidi()const override{return false;}
    bool producesMidi()const override{return false;} bool hasEditor()const override{return false;}
    juce::AudioProcessorEditor* createEditor()override{return nullptr;}
    int getNumPrograms()override{return 1;} int getCurrentProgram()override{return 0;}
    void setCurrentProgram(int)override{} const juce::String getProgramName(int)override{return {};}
    void changeProgramName(int,const juce::String&)override{}
    void getStateInformation(juce::MemoryBlock&)override{} void setStateInformation(const void*,int)override{}
private: MasterOutputService& output;
};

// Resolve the named, verified percentage parameter, never a version-specific
// parameter index. VST3 setValueNotifyingHost queues changes in the normal host
// parameter path; no plugin state reload is used while dragging the slider.
inline juce::AudioProcessorParameter* proR2MixParameter(juce::AudioProcessor& processor) {
    for(auto* p:processor.getParameters()) if(p && p->getName(128).trim().equalsIgnoreCase("Mix")) {
        const auto number=[](juce::String s){return s.retainCharacters("0123456789.-").getFloatValue();};
        if(std::abs(number(p->getText(0.f,64)))<.01f
            && std::abs(number(p->getText(.2f,64))-20.f)<.1f
            && std::abs(number(p->getText(1.f,64))-100.f)<.1f) return p;
    }
    return nullptr;
}
}
