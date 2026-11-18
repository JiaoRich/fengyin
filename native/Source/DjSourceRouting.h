#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "SwamStateCompatibility.h"
#include "SerialEffectRouting.h"
#include "DjSourceProfile.h"
namespace fengyin
{
// Permit forward Pro-R 2 minor-version restores, never send a newer state
// to an older installed plugin. Exact tested package/runtime version is 2.06.
inline bool djEffectVersionCompatible(const juce::PluginDescription& required,const juce::PluginDescription& candidate)
{
    return required.version==candidate.version
        || (required.manufacturerName=="FabFilter" && candidate.manufacturerName=="FabFilter"
            && required.name=="Pro-R 2" && candidate.name==required.name
            && required.version.getIntValue()==2 && candidate.version.getIntValue()==2
            && candidate.version.compareNatural(required.version)>=0);
}
struct DjMidiRoute
{
    // channel 17 (OMNI) is driven on the host's existing channel 1.
    static int encode(const juce::XmlElement& e) {
        const int type=e.getIntAttribute("messageType"),msb=e.getIntAttribute("msb",-1),lsb=e.getIntAttribute("lsb",-1);
        const int ch=e.getIntAttribute("channel",17);
        if(!e.getBoolAttribute("enabled") || (type!=1&&type!=2) || msb<0 || msb>127 || ch<1 || ch>17
            || (type==2&&(msb>31||lsb!=msb+32)))return 0;
        return ((ch==17?1:ch)<<8)|(msb+1)|(type==2?((lsb+1)<<16):0);
    }
    static int channel(int route){return (route>>8)&255;}
    static int coarse(int route){return (route&255)-1;}
    static int fine(int route){return ((route>>16)&255)-1;}
    static bool overlaps(int a,int b) {
        if(!a||!b||channel(a)!=channel(b))return false;
        return coarse(a)==coarse(b)||coarse(a)==fine(b)||fine(a)==coarse(b)||(fine(a)>=0&&fine(a)==fine(b));
    }
    static int find(const juce::XmlElement& root,const juce::String& parameter) {
        const auto* table=SwamStateCompatibility::table(root); if(!table)return 0;
        int result=0;
        for(auto* e:table->getChildIterator())if(e->getStringAttribute("parameterId").equalsIgnoreCase(parameter)) {
            const int r=encode(*e);if(r){if(result&&result!=r)return 0;result=r;}
        }
        return result;
    }
};
// REAPER track gain is before its post-fader send, not after the reverb.
class DjFixedGainProcessor final: public juce::AudioProcessor
{
public:
    explicit DjFixedGainProcessor(float gain):AudioProcessor(BusesProperties().withInput("In",juce::AudioChannelSet::stereo(),true).withOutput("Out",juce::AudioChannelSet::stereo(),true)),level(gain){}
    const juce::String getName() const override{return "DJ source bus gain";}
    void prepareToPlay(double,int)override{} void releaseResources()override{}
    void processBlock(juce::AudioBuffer<float>& b,juce::MidiBuffer&)override{b.applyGain(level);}
    double getTailLengthSeconds()const override{return 0;} bool acceptsMidi()const override{return false;} bool producesMidi()const override{return false;}
    bool hasEditor()const override{return false;} juce::AudioProcessorEditor* createEditor()override{return nullptr;}
    int getNumPrograms()override{return 1;} int getCurrentProgram()override{return 0;} void setCurrentProgram(int)override{}
    const juce::String getProgramName(int)override{return {}; } void changeProgramName(int,const juce::String&)override{}
    void getStateInformation(juce::MemoryBlock&)override{} void setStateInformation(const void*,int)override{}
private:float level;
};

inline bool connectDjSourceGraph(juce::AudioProcessorGraph& graph,
    juce::AudioProcessorGraph::Node::Ptr instrument,
    const std::vector<juce::AudioProcessorGraph::Node::Ptr>& preEffects,
    juce::AudioProcessorGraph::Node::Ptr track,
    juce::AudioProcessorGraph::Node::Ptr reverb,
    juce::AudioProcessorGraph::Node::Ptr bus,
    juce::AudioProcessorGraph::Node::Ptr hardware,
    juce::AudioProcessorGraph::Node::Ptr output, bool direct)
{
    bool ok=connectSerialEffects(graph,instrument,preEffects,track);
    ok=connectSerialEffects(graph,track,{reverb},bus)&&ok;
    ok=connectSerialEffects(graph,bus,{},hardware)&&ok;
    if(direct)for(int c=0;c<2;++c)
        ok=graph.addConnection({{track->nodeID,c},{hardware->nodeID,c}})&&ok;
    return connectSerialEffects(graph,hardware,{},output)&&ok;
}
}
