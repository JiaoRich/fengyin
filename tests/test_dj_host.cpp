#include "PluginHostEngine.h"
#include "TonePackage.h"
#include "ToneStyleCatalog.h"
#include "PublishedToneCatalog.h"
#include "PerformanceReverb.h"
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
    void processBlock(juce::AudioBuffer<float>& b,juce::MidiBuffer&)override{for(int c=0;c<b.getNumChannels();++c)for(int n=0;n<b.getNumSamples();++n)b.setSample(c,n,.1f);}
    bool acceptsMidi()const override{return true;} bool producesMidi()const override{return false;}
    double getTailLengthSeconds()const override{return 0;}
    bool hasEditor()const override{return false;}juce::AudioProcessorEditor* createEditor()override{return nullptr;}
    int getNumPrograms()override{return 1;}int getCurrentProgram()override{return 0;}void setCurrentProgram(int)override{}
    const juce::String getProgramName(int)override{return {}; }void changeProgramName(int,const juce::String&)override{}
    void getStateInformation(juce::MemoryBlock& out)override{out=state;}
    void setStateInformation(const void* p,int n)override{state.replaceAll(p,static_cast<size_t>(n));}
};
// A deterministic percentage effect checks production routing, not reverb synthesis.
class PercentEffect final : public juce::AudioProcessor {
public:
 PercentEffect():AudioProcessor(BusesProperties().withInput("In",juce::AudioChannelSet::stereo(),true).withOutput("Out",juce::AudioChannelSet::stereo(),true)) {
  mix=new juce::AudioParameterFloat("mix","Mix",juce::NormalisableRange<float>(0,100,.001f),20);addParameter(mix);
 }
 juce::AudioParameterFloat* mix;
 const juce::String getName()const override{return "FabFilter Pro-R 2";}
 void prepareToPlay(double,int)override{}void releaseResources()override{}
 void processBlock(juce::AudioBuffer<float>& b,juce::MidiBuffer&)override{b.applyGain(1.f-mix->get()/200.f);}
 bool acceptsMidi()const override{return false;}bool producesMidi()const override{return false;}
 double getTailLengthSeconds()const override{return 0;}bool hasEditor()const override{return false;}
 juce::AudioProcessorEditor* createEditor()override{return nullptr;}
 int getNumPrograms()override{return 1;}int getCurrentProgram()override{return 0;}void setCurrentProgram(int)override{}
 const juce::String getProgramName(int)override{return {};}void changeProgramName(int,const juce::String&)override{}
 void getStateInformation(juce::MemoryBlock& b)override{float v=static_cast<juce::AudioProcessorParameter*>(mix)->getValue();b.replaceAll(&v,sizeof(v));}
 void setStateInformation(const void* b,int n)override{assert(n==sizeof(float));float v;std::memcpy(&v,b,sizeof(v));mix->setValueNotifyingHost(v);}
};
namespace fengyin {
struct DjHostTestAccess {
    static SourceInstrument& install(PluginHostEngine& h,const SoundPreset& preset) {
        h.graph=std::make_unique<juce::AudioProcessorGraph>();h.graph->setPlayConfigDetails(0,2,48000,128);
        auto source=std::make_unique<SourceInstrument>(preset.instrumentState);auto* raw=source.get();
        h.instrumentNode=h.graph->addNode(std::move(source));
        h.audioOutputNode=h.graph->addNode(std::make_unique<juce::AudioProcessorGraph::AudioGraphIOProcessor>(juce::AudioProcessorGraph::AudioGraphIOProcessor::audioOutputNode));
        h.midiInputNode=h.graph->addNode(std::make_unique<juce::AudioProcessorGraph::AudioGraphIOProcessor>(juce::AudioProcessorGraph::AudioGraphIOProcessor::midiInputNode));
        juce::PluginDescription d;d.name="FabFilter Pro-R 2";d.manufacturerName="FabFilter";
        h.effectNodes.push_back({h.graph->addNode(std::make_unique<PercentEffect>()),d});
        return *raw;
    }
    static void checkReverb(PluginHostEngine& h,const SoundPreset& preset) {
        auto master=std::make_unique<MasterOutputService>();master->setSampleRate(48000);h.setMasterOutputService(master.get());
        h.configurePerformanceReverb(true);assert(h.rebuildConnections());
        assert(h.performanceReverbAvailable());assert(h.setPerformanceReverbMix(0));
        h.graph->prepareToPlay(48000,128);juce::AudioBuffer<float> audio(2,128);juce::MidiBuffer midi;
        for(int i=0;i<4;++i){audio.clear();h.graph->processBlock(audio,midi);}
        // Every new source, including legacy parallel track 7, has ONE dry path.
        const float expected=.1f*preset.djSource.trackGain*preset.djSource.hardwareGain;
        assert(std::abs(audio.getSample(0,20)-expected)<1e-5f);
        for(float value:{.2f,.8f,1.f,0.f}) {
            assert(h.setPerformanceReverbMix(value));assert(std::abs(h.getPerformanceReverbMix()-value)<1e-5f);
            auto states=h.captureEffectChain();assert(states.size()==1);
            h.setPerformanceReverbMix(.1f);h.effectNodes[0].node->getProcessor()->setStateInformation(states[0].state.getData(),int(states[0].state.getSize()));
            assert(std::abs(h.getPerformanceReverbMix()-value)<1e-5f);
        }
        assert(!h.setPerformanceReverbMix(std::numeric_limits<float>::quiet_NaN()));
        assert(h.setEffectBypassedAt(0,true));assert(!h.performanceReverbAvailable());
        assert(h.setPerformanceReverbMix(.3f));assert(h.setEffectBypassedAt(0,false));assert(h.performanceReverbAvailable());
        h.configurePerformanceReverb(false);assert(h.rebuildConnections());
        assert(!h.setPerformanceReverbMix(.5f));
        // Old custom/DJ packages retain their prior direct-master behavior.
        for(int i=0;i<4;++i){audio.clear();h.graph->processBlock(audio,midi);}
        assert(std::abs(audio.getSample(0,20)-expected*(.85f+(preset.djSource.directToMaster?1.f:0.f)))<1e-5f);
        h.graph->releaseResources();h.setMasterOutputService(nullptr);
    }
    static void checkOrdinary(PluginHostEngine& h) {
        auto master=std::make_unique<MasterOutputService>(),reference=std::make_unique<MasterOutputService>();
        master->setSampleRate(48000);reference->setSampleRate(48000);master->setReverbMix(0);reference->setReverbMix(0);
        master->setEqTone(.35f);reference->setEqTone(.35f);h.setMasterOutputService(master.get());
        h.configurePerformanceReverb(true);assert(h.rebuildConnections());assert(h.instrumentToneNode);
        assert(h.player.instrumentEffectsInGraph.load());assert(h.setPerformanceReverbMix(0));
        const auto& rev=h.effectNodes.back().node;
        assert(h.graph->isConnected({{h.instrumentToneNode->nodeID,0},{rev->nodeID,0}}));
        h.graph->prepareToPlay(48000,128);juce::MidiBuffer midi;juce::AudioBuffer<float> a(2,128),b(2,128);
        for(int block=0;block<100;++block) {
            a.clear();for(int c=0;c<2;++c)for(int n=0;n<128;++n)b.setSample(c,n,.1f);
            h.graph->processBlock(a,midi);master->processInstrumentOutput(a.getArrayOfWritePointers(),2,128);
            reference->processInstrument(b.getArrayOfWritePointers(),2,128);
            for(int c=0;c<2;++c)for(int n=0;n<128;++n)assert(a.getSample(c,n)==b.getSample(c,n));
        }
        assert(h.setEffectBypassedAt(0,true));assert(h.player.instrumentEffectsInGraph.load());
        h.graph->releaseResources();h.setMasterOutputService(nullptr);
    }
    static void checkActual(const juce::PluginDescription& description,const juce::MemoryBlock& bytes) {
        auto master=std::make_unique<MasterOutputService>();master->setSampleRate(48000);
        auto host=std::make_unique<PluginHostEngine>();SoundPreset preset;install(*host,preset);
        host->setMasterOutputService(master.get());host->configurePerformanceReverb(true);
        EffectChainStates states;EffectChainState entry;entry.descriptionXml=description.createXml()->toString();entry.state=bytes;states.push_back(entry);
        const auto restore=[&](const EffectChainStates& saved){
            bool done=false,success=false;host->restoreEffectChain(saved,48000,128,[&](bool ok,const juce::String&){success=ok;done=true;});
            const auto start=juce::Time::getMillisecondCounter();
            while(!done && juce::Time::getMillisecondCounter()-start<15000)juce::MessageManager::getInstance()->runDispatchLoopUntil(10);
            assert(done && success);assert(host->usesPerformanceReverb());
        };
        restore(states);assert(host->performanceReverbAvailable());
        host->graph->prepareToPlay(48000,128);juce::AudioBuffer<float> b(2,128);juce::MidiBuffer m;
        for(float value:{0.f,.2f,.8f,1.f}) {
            assert(host->setPerformanceReverbMix(value));
            const float controller=host->getPerformanceReverbMix();assert(std::abs(controller-value)<.0001f);
            // Deliberately save before an audio block: metadata must restore the
            // current controller even if the VST3 component still has the old value.
            auto saved=host->captureEffectChain();restore(saved);assert(host->setPerformanceReverbMix(controller));
            assert(std::abs(host->getPerformanceReverbMix()-value)<.0001f);
            for(int block=0;block<32;++block){b.clear();host->graph->processBlock(b,m);for(int c=0;c<2;++c)for(int n=0;n<128;++n)assert(std::isfinite(b.getSample(c,n)));}
        }
        assert(host->setEffectBypassedAt(0,true));auto bypassed=host->captureEffectChain();restore(bypassed);
        assert(!host->performanceReverbAvailable());assert(host->setPerformanceReverbMix(.8f));
        assert(host->setEffectBypassedAt(0,false));assert(std::abs(host->getPerformanceReverbMix()-.8f)<.0001f);
        auto invalid=host->captureEffectChain();invalid[0].descriptionXml="invalid";bool rejected=false;
        host->restoreEffectChain(invalid,48000,128,[&](bool ok,const juce::String&){rejected=!ok;});
        assert(rejected && host->performanceReverbAvailable());assert(std::abs(host->getPerformanceReverbMix()-.8f)<.0001f);
        assert(!host->removeEffectAt(0));host->unloadEffect();assert(host->performanceReverbAvailable());
        host->graph->releaseResources();host->setMasterOutputService(nullptr);
    }
    static std::vector<RealtimeMidiEvent> drain(PluginHostEngine& h){
        std::vector<RealtimeMidiEvent> out;RealtimeMidiEvent e;while(h.player.midiQueue.pop(e))out.push_back(e);return out;
    }
};
}
void testActualReverbHost(const juce::PluginDescription& d,const juce::MemoryBlock& b) {fengyin::DjHostTestAccess::checkActual(d,b);}
void testPerformanceResetDelivery();
void testRealPluginResetDelivery(const juce::String& pluginPath);
void testPerformanceReverbPlugin(const juce::String&,const juce::String&);
int main(int argc,char** argv){
    juce::ScopedJuceInitialiser_GUI init;
    if(argc==4 && juce::String(argv[1])=="--reverb-plugin"){testPerformanceReverbPlugin(juce::String::fromUTF8(argv[2]),juce::String::fromUTF8(argv[3]));return 0;}
    if(argc==3 && juce::String(argv[1])=="--plugin-reset"){
        testRealPluginResetDelivery(juce::String::fromUTF8(argv[2]));return 0;
    }
    {juce::PluginDescription a,b;a.name=b.name="Pro-R 2";a.manufacturerName=b.manufacturerName="FabFilter";
     a.version="2.06";b.version="2.00";assert(!fengyin::djEffectVersionCompatible(a,b));
     b.version="2.06";assert(fengyin::djEffectVersionCompatible(a,b));b.version="2.07";assert(fengyin::djEffectVersionCompatible(a,b));
     a.version="2.00";b.version="2.06";assert(fengyin::djEffectVersionCompatible(a,b));
     for(const auto* required:{"2.06","2.0.6","2.0.6.0"}){
         a.version=required;
         for(const auto* installed:{"2.06","2.0.6","2.0.6.0","2.0.6.25","2.07","2.0.7.0","2.10","2.1.0.0"}){
             b.version=installed;assert(fengyin::djEffectVersionCompatible(a,b));
         }
         for(const auto* installed:{"2.00","2.0.0.0","2.05","2.0.5.25","1.0.6.0","3.0.0.0","2.0.6-beta","2..6.0",""}){
             b.version=installed;assert(!fengyin::djEffectVersionCompatible(a,b));
         }
     }
     a.version="2.00";b.version="2.0.6.25";assert(fengyin::djEffectVersionCompatible(a,b));
     a.version="2.0.6.25";b.version="2.0.6.0";assert(!fengyin::djEffectVersionCompatible(a,b));
     a.manufacturerName=b.manufacturerName="Other";a.version="2.06";b.version="2.0.6.0";
     assert(!fengyin::djEffectVersionCompatible(a,b));b.version=a.version;assert(fengyin::djEffectVersionCompatible(a,b));}
    testPerformanceResetDelivery();
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
        assert(host->captureInstrumentState()==p->instrumentState);fengyin::DjHostTestAccess::checkReverb(*host,*p);++count;
    }
    { fengyin::SoundPreset plain;auto ordinary=std::make_unique<fengyin::PluginHostEngine>();
      fengyin::DjHostTestAccess::install(*ordinary,plain);fengyin::DjHostTestAccess::checkOrdinary(*ordinary); }
    assert(count==29);std::cout<<"29 source states: production host routing, 7/14-bit delivery, legacy normalizers blocked, bus protection, capture passed\n";
}
