#include "PluginHostEngine.h"
#include "TonePackage.h"
#include "ToneStyleCatalog.h"
#include "PublishedToneCatalog.h"
#include <cassert>
#include <iostream>

// Test instrument retains the real source state and observes host-side parameter
// edits. It does not simulate SWAM synthesis: that requires the licensed plug-in.
class AttackParameter final: public juce::AudioPluginInstance::HostedParameter {
public:
    juce::String getParameterID()const override{return "attack";}
    float getValue()const override{return value;} void setValue(float v)override{value=v;}
    float getDefaultValue()const override{return 0;}
    juce::String getName(int)const override{return "Attack Control";}
    juce::String getLabel()const override{return {};}
    juce::String getText(float v,int)const override{return v>=.5f?"Expression":"Velocity";}
    float getValueForText(const juce::String& s)const override{return s=="Expression"?1.f:0.f;}
    int getIndex()const{return value>=.5f?1:0;}
private:float value=0;
};
class SourceInstrument final : public juce::AudioPluginInstance {
public:
    explicit SourceInstrument(juce::MemoryBlock bytes):AudioPluginInstance(BusesProperties().withOutput("Out",juce::AudioChannelSet::stereo(),true)),state(std::move(bytes)) {
        auto parameter=std::make_unique<AttackParameter>();attack=parameter.get();addHostedParameter(std::move(parameter));
    }
    AttackParameter* attack;juce::MemoryBlock state;
    void fillInPluginDescription(juce::PluginDescription& d) const override {d.name="SWAM Alto Sax";d.isInstrument=true;}
    const juce::String getName()const override{return "SWAM Alto Sax";}
    void prepareToPlay(double,int)override{} void releaseResources()override{}
    void processBlock(juce::AudioBuffer<float>& b,juce::MidiBuffer&)override{b.clear();}
    bool acceptsMidi()const override{return true;} bool producesMidi()const override{return false;}
    double getTailLengthSeconds()const override{return 0;}
    bool hasEditor()const override{return false;}juce::AudioProcessorEditor* createEditor()override{return nullptr;}
    int getNumPrograms()override{return 1;}int getCurrentProgram()override{return 0;}void setCurrentProgram(int)override{}
    const juce::String getProgramName(int)override{return {}; }void changeProgramName(int,const juce::String&)override{}
    void getStateInformation(juce::MemoryBlock& out)override{out=state;}
    void setStateInformation(const void* p,int n)override{state.replaceAll(p,static_cast<size_t>(n));}
};
namespace fengyin {
struct DjHostTestAccess {
    static SourceInstrument& install(PluginHostEngine& h,const SoundPreset& preset) {
        h.graph=std::make_unique<juce::AudioProcessorGraph>();h.graph->setPlayConfigDetails(0,2,48000,128);
        auto source=std::make_unique<SourceInstrument>(preset.instrumentState);auto* raw=source.get();
        h.instrumentNode=h.graph->addNode(std::move(source));
        h.audioOutputNode=h.graph->addNode(std::make_unique<juce::AudioProcessorGraph::AudioGraphIOProcessor>(juce::AudioProcessorGraph::AudioGraphIOProcessor::audioOutputNode));
        h.midiInputNode=h.graph->addNode(std::make_unique<juce::AudioProcessorGraph::AudioGraphIOProcessor>(juce::AudioProcessorGraph::AudioGraphIOProcessor::midiInputNode));
        juce::PluginDescription d;d.name="FabFilter Pro-R 2";
        h.effectNodes.push_back({h.graph->addNode(std::make_unique<DjFixedGainProcessor>(1)),d});
        return *raw;
    }
    static std::vector<RealtimeMidiEvent> drain(PluginHostEngine& h){
        std::vector<RealtimeMidiEvent> out;RealtimeMidiEvent e;while(h.player.midiQueue.pop(e))out.push_back(e);return out;
    }
};
}
int main(){
    juce::ScopedJuceInitialiser_GUI init;
    const auto root=juce::File(juce::String::fromUTF8(__FILE__)).getParentDirectory().getParentDirectory();
    int count=0;
    for(const auto& tone:fengyin::publishedToneCatalog){
        if(!juce::String(tone.id.data()).startsWith("dj-source-"))continue;
        juce::String error;
        auto p=fengyin::TonePackage::read(root.getChildFile("assets/tone-packages/"+juce::String(tone.assetName.data())),error);
        assert(p);auto host=std::make_unique<fengyin::PluginHostEngine>();
        auto& fake=fengyin::DjHostTestAccess::install(*host,*p);
        assert(host->configureDjSource(p->djSource));
        assert(host->applyExpressionAttackControl());assert(fake.attack->getIndex()==0);
        auto style=fengyin::ToneStyleCatalog::find("alto-sax","natural");
        assert(host->applySwamToneProfile(style.swam)==0);
        assert(host->applyStandardSwamExpressionCurve(false));assert(fake.attack->getIndex()==0);
        assert(host->captureInstrumentState()==p->instrumentState);
        assert(!host->removeEffectAt(0));assert(!host->moveEffect(0,0));
        host->unloadEffect();assert(host->hasEffect());assert(host->setEffectBypassedAt(0,true));assert(host->setEffectBypassedAt(0,false));
        fengyin::DjHostTestAccess::drain(*host);
        const auto xml=fengyin::SwamStateCompatibility::read(p->instrumentState);assert(xml);
        const int expression=fengyin::DjMidiRoute::find(*xml,"expression");assert(expression);
        const bool fine=fengyin::DjMidiRoute::fine(expression)>=0;
        for(float value:{0.f,.1234f,.5f,1.f,0.f}){
            host->breathChanged(value,10);auto events=fengyin::DjHostTestAccess::drain(*host);assert(events.size()==1);
            const auto& e=events.front();const int raw=juce::roundToInt(value*(fine?16383.f:127.f));
            assert(e.bytes[0]==0xb0+fengyin::DjMidiRoute::channel(expression)-1);
            assert(e.bytes[1]==fengyin::DjMidiRoute::coarse(expression));assert(e.size==(fine?6:3));
            assert(e.bytes[2]==(fine?raw>>7:raw));
            if(fine){assert(e.bytes[3]==e.bytes[0]&&e.bytes[4]==fengyin::DjMidiRoute::fine(expression)&&e.bytes[5]==(raw&127));}
            host->breathChanged(value,11);assert(fengyin::DjHostTestAccess::drain(*host).empty());
        }
        for(auto role:{fengyin::PerformanceTechnique::vibrato,fengyin::PerformanceTechnique::growl}){
            const bool capable=host->hasIndependentTechniqueRoute(role);
            for(float value:{0.f,.5f,1.f,0.f}){
                host->techniqueChanged(role,value);auto events=fengyin::DjHostTestAccess::drain(*host);
                assert(events.size()==(capable?1u:0u));
                if(capable){const auto& e=events[0];assert(e.bytes[2]==(juce::roundToInt(value*(e.size==6?16383.f:127.f))>>(e.size==6?7:0)));}
            }
        }
        assert(host->captureInstrumentState()==p->instrumentState);++count;
    }
    assert(count==29);std::cout<<"29 source states: production host routing, 7/14-bit delivery, legacy normalizers blocked, bus protection, capture passed\n";
}
