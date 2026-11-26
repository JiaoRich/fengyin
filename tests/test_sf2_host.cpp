#include "PluginHostEngine.h"
#include "TonePackage.h"
#include "InstrumentDisplayName.h"
#include "../native/Sf2/Sf2State.h"
#include <cassert>
#include <iostream>

namespace sf2_test {
class GainEffect final:public juce::AudioPluginInstance {
public:
 GainEffect():AudioPluginInstance(BusesProperties().withInput("In",juce::AudioChannelSet::stereo(),true).withOutput("Out",juce::AudioChannelSet::stereo(),true)){}
 void fillInPluginDescription(juce::PluginDescription& d)const override{d.name="Test gain";d.pluginFormatName="SF2 test effect";}
 const juce::String getName()const override{return "Test gain";}
 void prepareToPlay(double,int)override{}void releaseResources()override{}
 void processBlock(juce::AudioBuffer<float>& b,juce::MidiBuffer&)override{b.applyGain(gain);}
 bool acceptsMidi()const override{return false;}bool producesMidi()const override{return false;}
 double getTailLengthSeconds()const override{return 0;}bool hasEditor()const override{return false;}
 juce::AudioProcessorEditor* createEditor()override{return nullptr;}
 int getNumPrograms()override{return 1;}int getCurrentProgram()override{return 0;}void setCurrentProgram(int)override{}
 const juce::String getProgramName(int)override{return {};}void changeProgramName(int,const juce::String&)override{}
 void getStateInformation(juce::MemoryBlock& b)override{b.replaceAll(&gain,sizeof(gain));}
 void setStateInformation(const void* b,int n)override{assert(n==sizeof(gain));std::memcpy(&gain,b,sizeof(gain));}
 float gain=1;
};
class GainFormat final:public juce::AudioPluginFormat {
public:
 juce::String getName()const override{return "SF2 test effect";}
 void findAllTypesForFile(juce::OwnedArray<juce::PluginDescription>&,const juce::String&)override{}
 bool fileMightContainThisPluginType(const juce::String&)override{return true;}
 juce::String getNameOfPluginFromIdentifier(const juce::String& s)override{return s;}
 bool pluginNeedsRescanning(const juce::PluginDescription&)override{return false;}
 bool doesPluginStillExist(const juce::PluginDescription&)override{return true;}
 bool canScanForPlugins()const override{return false;}bool isTrivialToScan()const override{return true;}
 juce::StringArray searchPathsForPlugins(const juce::FileSearchPath&,bool,bool)override{return {};}
 juce::FileSearchPath getDefaultLocationsToSearch()override{return {};}
 bool requiresUnblockedMessageThreadDuringCreation(const juce::PluginDescription&)const override{return false;}
 void createPluginInstance(const juce::PluginDescription& d,double,int,PluginCreationCallback callback)override{
  if(d.name=="Fail")callback(nullptr,"Deliberate effect failure");else callback(std::make_unique<GainEffect>(),{});
 }
};

class Device final : public juce::AudioIODevice {
public:
    Device():AudioIODevice("Reset probe","Test"){}
    juce::StringArray getOutputChannelNames()override{return {"L","R"};}
    juce::StringArray getInputChannelNames()override{return {};}
    juce::Array<double> getAvailableSampleRates()override{return {48000.};}
    juce::Array<int> getAvailableBufferSizes()override{return {128};}
    int getDefaultBufferSize()override{return 128;}
    juce::String open(const juce::BigInteger&,const juce::BigInteger&,double,int)override{return {};}
    void close()override{}bool isOpen()override{return true;}
    void start(juce::AudioIODeviceCallback*)override{}void stop()override{}bool isPlaying()override{return true;}
    juce::String getLastError()override{return {};}
    int getCurrentBufferSizeSamples()override{return 128;}double getCurrentSampleRate()override{return 48000;}
    int getCurrentBitDepth()override{return 32;}
    juce::BigInteger getActiveOutputChannels()const override{return juce::BigInteger(3);}
    juce::BigInteger getActiveInputChannels()const override{return {};}
    int getOutputLatencyInSamples()override{return 0;}int getInputLatencyInSamples()override{return 0;}
};
}
namespace fengyin {
struct Sf2HostTestAccess {
 static void addTestEffect(PluginHostEngine& host){host.formatManager.addFormat(std::make_unique<sf2_test::GainFormat>());}
 static double render(PluginHostEngine& host,int blocks=200) {
    double energy=0;juce::AudioBuffer<float> audio(2,128);juce::MidiBuffer midi;
    for(int b=0;b<blocks;++b) {
        // Use production collector/player so queued controller reset/order are exercised.
        const juce::AudioIODeviceCallbackContext context{};
        host.player.audioDeviceIOCallbackWithContext(nullptr,0,audio.getArrayOfWritePointers(),2,128,context);
        for(int ch=0;ch<2;++ch)for(int n=0;n<128;++n){const auto v=audio.getSample(ch,n);assert(std::isfinite(v));if(b>blocks/2)energy+=v*v;}
    }
    return energy;
 }
 static void prepare(PluginHostEngine& host) {
    static sf2_test::Device device;
    host.player.audioDeviceAboutToStart(&device);
 }
};
}

void testSf2Host(const juce::String& pluginPath,const juce::File& source)
{
 using namespace fengyin;
 auto host=std::make_unique<PluginHostEngine>();
 juce::VST3PluginFormat format;juce::OwnedArray<juce::PluginDescription> list;
 format.findAllTypesForFile(list,pluginPath);assert(list.size()==1);
 auto description=*list[0];assert(instrumentDisplayName(description)==juce::String::fromUTF8("sf2格式音源"));
 juce::String hash;auto result=sf2::importResource(source,hash);if(result.failed())std::cerr<<result.getErrorMessage()<<std::endl;assert(result.wasOk());
 sf2::State state;state.hash=hash;state.name=source.getFileName();state.bytes=source.getSize();state.bank=0;state.program=juce::SystemStats::getEnvironmentVariable("FENGYIN_SF2_TEST_PROGRAM","0").getIntValue();
 auto load=[&](const juce::MemoryBlock& bytes,bool expected,std::optional<PluginHostEngine::Sf2LoadOptions> options={}) {
    bool done=false,success=false;juce::String error;
    host->loadAsync(description,48000,128,bytes,[&](bool ok,const juce::String& message){success=ok;error=message;done=true;},false,false,options);
    const auto deadline=juce::Time::getMillisecondCounterHiRes()+45000;
    while(!done && juce::Time::getMillisecondCounterHiRes()<deadline)juce::MessageManager::getInstance()->runDispatchLoopUntil(10);
    if(!done || success!=expected)std::cerr<<"load failed: "<<error<<" done="<<done<<std::endl;
    assert(done && success==expected);
 };
 const auto vstState=[](const sf2::State& state) {
    juce::XmlElement root("VST3PluginState");
    root.createNewChildElement("IComponent")->addTextElement(state.encode().toBase64Encoding());
    juce::MemoryBlock bytes;juce::AudioProcessor::copyXmlToBinary(root,bytes);return bytes;
 };
 load(vstState(state),true);
 auto* original=host->getPlugin();assert(original);
 Sf2HostTestAccess::prepare(*host);
 Sf2HostTestAccess::render(*host,5);
 auto captured=host->captureInstrumentState();auto decoded=sf2::State::decode(captured.getData(),int(captured.getSize()));
 assert(decoded && decoded->hasResource() && decoded->ready && decoded->bytes==state.bytes);
 auto stamp=[] {return juce::Time::getMillisecondCounterHiRes()*.001;};
 host->breathChanged(.8f,stamp());host->noteOn(69,.8f,stamp());
 const auto loud=Sf2HostTestAccess::render(*host);assert(loud>0.00001);
 host->noteOn(71,.8f,stamp());host->noteOff(69,stamp());host->breathChanged(.3f,stamp());
 const auto soft=Sf2HostTestAccess::render(*host);assert(soft<loud*.5 && soft>0);
 host->breathChanged(0,stamp());host->noteOff(71,stamp());assert(Sf2HostTestAccess::render(*host)<loud*.00001);
 captured=host->captureInstrumentState();decoded=sf2::State::decode(captured.getData(),int(captured.getSize()));assert(decoded&&decoded->ready);
 SoundPreset preset;preset.id=juce::Uuid().toString();preset.name="SF2 roundtrip";preset.pluginBrand="sf2";preset.instrumentKey="sf2-engine";
 preset.instrumentChineseName=juce::String::fromUTF8("测试笛");preset.pluginIdentifier=description.createIdentifierString();
 preset.instrumentDescriptionXml=description.createXml()->toString();preset.instrumentState=captured;
 juce::TemporaryFile pack(".fytonepack"),bundle(".zip");assert(TonePackage::write(preset,pack.getFile()));
 juce::String error;auto read=TonePackage::read(pack.getFile(),error);if(!read)std::cerr<<"package read: "<<error<<std::endl;assert(read && read->instrumentState==captured);
 assert(TonePackage::writeWithSf2(preset,bundle.getFile()));juce::ZipFile z(bundle.getFile());assert(z.getNumEntries()==3);
 assert(z.getIndexOfFileName(juce::String::fromUTF8("音色方案.fytonepack"))>=0);
 assert(z.getIndexOfFileName(juce::String::fromUTF8("使用说明.txt"))>=0);
 load(read->instrumentState,true);Sf2HostTestAccess::prepare(*host);Sf2HostTestAccess::render(*host,5);
 host->breathChanged(.8f,stamp());host->noteOn(69,.8f,stamp());const auto reloaded=Sf2HostTestAccess::render(*host);assert(std::abs(loud-reloaded)<loud*.01);
 host->resetPerformance();Sf2HostTestAccess::render(*host);
 original=host->getPlugin();auto invalid=state;invalid.hash=juce::String::repeatedString("0",64);
 load(vstState(invalid),false);assert(host->getPlugin()==original);
 host->breathChanged(.8f,stamp());host->noteOn(69,.8f,stamp());assert(Sf2HostTestAccess::render(*host)>0);
 host->resetPerformance();Sf2HostTestAccess::render(*host);
 // Actual SF2 VST3 + deterministic effect through production async graph transaction.
 Sf2HostTestAccess::addTestEffect(*host);
 juce::PluginDescription fx;fx.name="Test gain";fx.pluginFormatName="SF2 test effect";fx.uniqueId=98217;
 EffectChainState effect;effect.descriptionXml=fx.createXml()->toString();float half=.5f;effect.state.replaceAll(&half,sizeof(half));
 PluginHostEngine::Sf2LoadOptions options;options.effects={effect,effect};
 load(read->instrumentState,true,options);Sf2HostTestAccess::prepare(*host);Sf2HostTestAccess::render(*host,5);
 host->breathChanged(.8f,stamp());host->noteOn(69,.8f,stamp());const auto withEffects=Sf2HostTestAccess::render(*host);
 assert(std::abs(withEffects-loud/16)<loud*.001);
 assert(host->captureEffectChain().size()==2 && host->captureEffectChain()[0].state==effect.state);
 // A ready SF2 with a broken effect must retain the old whole chain and sound.
 host->resetPerformance();Sf2HostTestAccess::render(*host);original=host->getPlugin();
 fx.name="Fail";options.effects[1].descriptionXml=fx.createXml()->toString();load(read->instrumentState,false,options);
 assert(host->getPlugin()==original && host->captureEffectChain().size()==2);
 host->breathChanged(.8f,stamp());host->noteOn(69,.8f,stamp());assert(Sf2HostTestAccess::render(*host)>0);
 // Held phrase must remain audible while the ready candidate waits for release.
 bool done=false;host->loadAsync(description,48000,128,read->instrumentState,[&](bool ok,const juce::String&){assert(ok);done=true;});
 const auto waitUntil=juce::Time::getMillisecondCounterHiRes()+1800;
 while(juce::Time::getMillisecondCounterHiRes()<waitUntil){Sf2HostTestAccess::render(*host,1);juce::MessageManager::getInstance()->runDispatchLoopUntil(5);}
 assert(!done && host->getPlugin()==original && Sf2HostTestAccess::render(*host)>0);
 host->resetPerformance();Sf2HostTestAccess::render(*host);
 const auto deadline=juce::Time::getMillisecondCounterHiRes()+10000;
 while(!done && juce::Time::getMillisecondCounterHiRes()<deadline)juce::MessageManager::getInstance()->runDispatchLoopUntil(5);
 assert(done && host->captureEffectChain().empty());
 const auto reverbPath=juce::SystemStats::getEnvironmentVariable("FENGYIN_SF2_TEST_REVERB",{});
 if(reverbPath.isNotEmpty()) {
    juce::OwnedArray<juce::PluginDescription> reverbTypes;format.findAllTypesForFile(reverbTypes,reverbPath);
    assert(reverbTypes.size()==1);
    PluginHostEngine::Sf2LoadOptions realEffects;EffectChainState reverbState;
    reverbState.descriptionXml=reverbTypes[0]->createXml()->toString();realEffects.effects={reverbState};
    auto master=std::make_unique<MasterOutputService>();master->setSampleRate(48000);master->setReverbMix(0);
    host->setMasterOutputService(master.get());realEffects.performanceReverb=true;realEffects.reverbMix=.2f;
    load(read->instrumentState,true,realEffects);Sf2HostTestAccess::prepare(*host);Sf2HostTestAccess::render(*host,5);
    assert(host->performanceReverbAvailable());assert(std::abs(host->getPerformanceReverbMix()-.2f)<.00001f);
    host->breathChanged(.8f,stamp());host->noteOn(69,.8f,stamp());assert(Sf2HostTestAccess::render(*host)>0);
    assert(host->setPerformanceReverbMix(.35f));realEffects.reverbMix=.35f;realEffects.effects=host->captureEffectChain();
    assert(realEffects.effects.size()==1 && realEffects.effects[0].state.getSize()>0);
    host->resetPerformance();Sf2HostTestAccess::render(*host);
    load(read->instrumentState,true,realEffects);Sf2HostTestAccess::prepare(*host);Sf2HostTestAccess::render(*host,5);
    assert(std::abs(host->getPerformanceReverbMix()-.35f)<.00001f);
    host->breathChanged(.8f,stamp());host->noteOn(69,.8f,stamp());assert(Sf2HostTestAccess::render(*host)>0);
    host->unload();host->setMasterOutputService(nullptr);
    std::cout<<"SF2 actual Pro-R2 complete chain, PCM, Mix and full effect-state reload passed\n";
 }
 host->unload();
 std::cout<<"SF2 real VST3/production host: legato CC11, zero, state/package/bundle, reload PCM and failed-candidate/whole-effect-chain retention and held-phrase commit passed\n";
}
