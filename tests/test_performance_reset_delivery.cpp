#include "PluginHostEngine.h"
#include "TechniqueProcessingGraph.h"
#include "ScopedGraphPause.h"
#include "MidiInputService.h"
#include "AudioStreamRecovery.h"
#include <set>
#include <cassert>
#include <future>
#include <iostream>

namespace reset_delivery_test {
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
class Instrument final : public juce::AudioProcessor {
public:
    Instrument():AudioProcessor(BusesProperties().withOutput("Out",juce::AudioChannelSet::stereo(),true)){received.reserve(20000);}
    int pitch=15000, bendCount=0;
    std::vector<juce::MidiMessage> received;
    const juce::String getName()const override{return "Reset observer";}
    void prepareToPlay(double,int)override{}void releaseResources()override{}
    void processBlock(juce::AudioBuffer<float>& b,juce::MidiBuffer& midi)override{
        b.clear();for(auto e:midi){auto m=e.getMessage();received.push_back(m);if(m.isPitchWheel()){pitch=m.getPitchWheelValue();++bendCount;}}
    }
    bool acceptsMidi()const override{return true;}bool producesMidi()const override{return false;}
    double getTailLengthSeconds()const override{return 0;}bool hasEditor()const override{return false;}
    juce::AudioProcessorEditor* createEditor()override{return nullptr;}
    int getNumPrograms()override{return 1;}int getCurrentProgram()override{return 0;}void setCurrentProgram(int)override{}
    const juce::String getProgramName(int)override{return {}; }void changeProgramName(int,const juce::String&)override{}
    void getStateInformation(juce::MemoryBlock&)override{}void setStateInformation(const void*,int)override{}
};
}
namespace fengyin {
struct PluginHostEngineTestAccess {
    static reset_delivery_test::Instrument& install(PluginHostEngine& h,reset_delivery_test::Device& d) {
        auto graph=std::make_unique<TechniqueProcessingGraph>();
        graph->preparePerformanceMidi=[&h](juce::MidiBuffer& midi){h.player.preparePerformanceMidi(midi);};
        h.graph=std::move(graph);h.graph->setPlayConfigDetails(0,2,48000,128);
        auto instrument=std::make_unique<reset_delivery_test::Instrument>();auto* raw=instrument.get();
        h.instrumentNode=h.graph->addNode(std::move(instrument));
        h.audioOutputNode=h.graph->addNode(std::make_unique<juce::AudioProcessorGraph::AudioGraphIOProcessor>(juce::AudioProcessorGraph::AudioGraphIOProcessor::audioOutputNode));
        h.midiInputNode=h.graph->addNode(std::make_unique<juce::AudioProcessorGraph::AudioGraphIOProcessor>(juce::AudioProcessorGraph::AudioGraphIOProcessor::midiInputNode));
        assert(h.rebuildConnections());h.player.setProcessor(h.graph.get());h.player.audioDeviceAboutToStart(&d);
        juce::MessageManager::getInstance()->runDispatchLoopUntil(10);return *raw;
    }
    static float block(PluginHostEngine& h){
        float left[128]{},right[128]{};float* output[]{left,right};
        h.player.audioDeviceIOCallbackWithContext(nullptr,0,output,2,128,{});
        float peak=0;for(float x:left)peak=std::max(peak,std::abs(x));return peak;
    }
    static RecordingAudioProcessorPlayer& player(PluginHostEngine& h){return h.player;}
    static void start(PluginHostEngine& h,reset_delivery_test::Device& d){h.player.audioDeviceAboutToStart(&d);}
    static juce::AudioProcessorGraph& graph(PluginHostEngine& h){return *h.graph;}
    static void holdChangeGate(PluginHostEngine& h,std::promise<void>& entered,std::future<void>& release){
        h.player.processorChangeGate.change([&]{entered.set_value();release.wait();});
    }
    static void cache(PluginHostEngine& h,const juce::PluginDescription& d,std::unique_ptr<juce::AudioPluginInstance> p) {
        auto e=std::make_shared<PluginHostEngine::CachedContainer>();e->instance=std::move(p);h.containerCache[d.createIdentifierString()]=e;
    }
    static void stop(PluginHostEngine& h){h.player.setProcessor(nullptr);h.player.audioDeviceStopped();}
};
}

namespace fengyin {
struct MidiInputServiceTestAccess {
    static void send(MidiInputService& s, juce::MidiMessage message) {
        message.setTimeStamp(juce::Time::getMillisecondCounterHiRes()*.001);
        s.handleIncomingMidiMessage(nullptr,message);
    }
    static bool timeout(MidiInputService& s, double elapsed) {
        return s.checkActiveSensing(s.lastMessageAtMs.load()+elapsed);
    }
    static bool timeoutWhileReceiving(MidiInputService& s) {
        const juce::SpinLock::ScopedLockType lock(s.incomingMessageLock);
        return timeout(s,10000);
    }
};
}
void testInputStreamRecovery() {
    using H=fengyin::PluginHostEngineTestAccess;
    using I=fengyin::MidiInputServiceTestAccess;
    reset_delivery_test::Device device;
    auto host=std::make_unique<fengyin::PluginHostEngine>();
    auto& observer=H::install(*host,device);
    auto input=std::make_unique<fengyin::MidiInputService>(false);
    input->setPerformanceSink(host.get());
    auto send=[&](const auto& m){I::send(*input,m);};
    auto block=[&]{H::block(*host);};
    auto held=[&]{std::set<int> notes;for(auto& m:observer.received){
        if(m.isNoteOn())notes.insert(m.getNoteNumber());
        if(m.isNoteOff())notes.erase(m.getNoteNumber());
        if(m.isAllSoundOff()||m.isAllNotesOff())notes.clear();}return notes;};
    auto reset=[&]{input->disconnect();block();observer.received.clear();};
    auto note=[&](int channel,int pitch){send(juce::MidiMessage::noteOn(channel,pitch,juce::uint8(100)));};
    auto breath=[&]{send(juce::MidiMessage::controllerEvent(1,2,90));};
    reset();breath();note(1,60);block();
    // A controller which sends only on change may hold a legal note for hours.
    assert(!I::timeout(*input,3600000));block();assert(held()==std::set<int>{60});
    send(juce::MidiMessage(0xfe));assert(!I::timeout(*input,349));
    assert(!I::timeoutWhileReceiving(*input));assert(!I::timeout(*input,349));
    // Any actual MIDI traffic, not only FE, keeps Active Sensing alive.
    for(int i=0;i<1000;++i){breath();assert(!I::timeout(*input,200));block();}
    assert(I::timeout(*input,351));block();assert(held().empty());
    assert(input->getSnapshot().breath==0&&input->getSnapshot().lastNote==-1);
    assert(input->getSnapshot().sensingTimeouts==1);assert(!I::timeout(*input,10000));
    breath();note(1,62);block();assert(held()==std::set<int>{62});
    send(juce::MidiMessage::noteOff(1,62));block();assert(held().empty());
    // BLE lifetime remains the GATT service's responsibility, including idle FE.
    input->attachExternal("test-ble","YDS-150 MIDI");breath();note(1,60);send(juce::MidiMessage(0xfe));block();
    juce::Thread::sleep(400);input->tickTechniqueAutomation();block();
    assert(input->isExternalConnected()&&held()==std::set<int>{60});
    reset();
    for(int cc:{120,123}) {
        breath();note(1,60);note(2,62);block();
        send(juce::MidiMessage::controllerEvent(1,cc,0));block();
        assert(held()==std::set<int>{62});assert(input->getSnapshot().lastNote==62);
        observer.received.clear();
        send(juce::MidiMessage::controllerEvent(2,cc,0));block();
        assert(input->getSnapshot().lastNote==-1);
        const auto hard=std::count_if(observer.received.begin(),observer.received.end(),[](auto& m){return m.isAllSoundOff();});
        assert((hard>0)==(cc==120)); // CC123 must not truncate normal release tails.
        reset();
    }
    // Repeated same pitch, transpose and overlapping channels don't release
    // the other channel's current note on a channel-mode message.
    note(1,60);note(2,60);block();send(juce::MidiMessage::allNotesOff(1));block();
    assert(held()==std::set<int>{60});send(juce::MidiMessage::noteOff(2,60));block();assert(held().empty());
    for(int delay:{5,180,250,1200}) {
        reset();breath();note(1,60);block();
        {fengyin::ScopedGraphPause pause(&H::graph(*host));
         note(1,62);send(juce::MidiMessage::noteOff(1,60));send(juce::MidiMessage::noteOff(1,62));
         block();juce::Thread::sleep(delay);block();}
        breath();note(1,64);block();
        assert(held()==std::set<int>{64});
        std::vector<int> notes;for(auto& m:observer.received)
            if(m.isNoteOnOrOff())notes.push_back((m.isNoteOn()?1:-1)*(m.getNoteNumber()+1));
        assert((notes==std::vector<int>{61,63,-61,-63,65}));
        send(juce::MidiMessage::noteOff(1,64));block();assert(held().empty());
    }
    assert(host->getLateMidiEventCount()>0&&host->getDroppedMidiEventCount()==0);
    reset();for(int n=0;n<2000;++n){note(1,60);block();send(juce::MidiMessage::noteOn(1,60,juce::uint8(0)));block();}
    assert(held().empty());input->setPerformanceSink(nullptr);H::stop(*host);
    std::cout<<"Production input/host/collector/graph recovery: held notes, heartbeat, CC120/123, BLE, delayed legato and 2000 normal phrases passed\n";
}
void testAudioStreamRecoveryPolicy() {
    using P=fengyin::AudioStreamRecovery;using A=P::Action;
    P p;assert(p.poll(0,false,false,true)==A::wait);
    for(int n=0;n<10000;++n)assert(p.poll(n,true,true,true)==A::render);
    assert(p.retryCount()==0);
    assert(p.poll(11000,true,false,true)==A::wait);assert(p.poll(11500,true,true,true)==A::render);
    assert(p.retryCount()==0);assert(p.poll(12000,true,false,true)==A::wait);
    assert(p.poll(13000,true,false,true)==A::retry);
    assert(p.poll(13999,true,false,true)==A::wait);assert(p.poll(14000,true,false,true)==A::retry);
    assert(p.poll(15999,true,false,true)==A::wait);assert(p.poll(16000,true,false,true)==A::retry);
    assert(p.poll(20000,true,false,true)==A::exhausted);
    assert(p.poll(20001,false,false,true)==A::wait);
    p.reset();assert(p.poll(0,true,false,false)==A::exhausted);assert(p.retryCount()==0);
    p.reset();assert(p.poll(0,true,false,true)==A::wait);assert(p.poll(1000,true,false,true)==A::retry);
    assert(p.poll(1001,true,true,true)==A::render);assert(p.poll(31001,true,true,true)==A::render);assert(p.retryCount()==0);
    std::cout<<"Audio stream policy: healthy silence, startup/transient grace, bounded retry, stop/logoff and stable rearm passed\n";
}

// Optional local/Windows verification with a licensed SWAM installation. Build
// a fresh state using that installed version; never cross-load a newer tone.
void testRealPluginResetDelivery(const juce::String& pluginPath){
    using Access=fengyin::PluginHostEngineTestAccess;
    juce::VST3PluginFormat format;juce::OwnedArray<juce::PluginDescription> descriptions;
    format.findAllTypesForFile(descriptions,pluginPath);assert(!descriptions.isEmpty());
    juce::String error;auto plugin=format.createInstanceFromDescription(*descriptions[0],48000,128,error);assert(plugin);
    std::cout<<"Actual reset plugin: "<<descriptions[0]->name<<" "<<descriptions[0]->version<<std::endl;
    juce::MessageManager::getInstance()->runDispatchLoopUntil(1000);
    plugin->setNonRealtime(true);plugin->setPlayConfigDetails(0,2,48000,128);plugin->prepareToPlay(48000,128);
    juce::AudioBuffer<float> audio(2,128);audio.clear();juce::MidiBuffer midi;
    midi.addEvent(juce::MidiMessage::pitchWheel(1,15000),0);plugin->processBlock(audio,midi);
    juce::MessageManager::getInstance()->runDispatchLoopUntil(50);
    juce::MemoryBlock bent;plugin->getStateInformation(bent);plugin->releaseResources();
    auto bend=[](const juce::MemoryBlock& bytes){
        auto xml=fengyin::SwamStateCompatibility::read(bytes);assert(xml);
        std::map<juce::String,double> params;fengyin::SwamStateCompatibility::params(*xml,params);
        assert(params.contains("pitchBend"));return params["pitchBend"];
    };
    assert(bend(bent)>.5);
    reset_delivery_test::Device device;auto host=std::make_unique<fengyin::PluginHostEngine>();
    Access::start(*host,device);
    // Reuse the one locally licensed instance that produced the state. This
    // covers the production cached-instance load, not a second cold activation.
    Access::cache(*host,*descriptions[0],std::move(plugin));
    bool loaded=false;
    host->loadAsync(*descriptions[0],48000,128,bent,[&](bool ok,const juce::String&){
        assert(ok);host->resetPerformance();assert(Access::graph(*host).isSuspended());
        Access::block(*host);loaded=true;
    });
    const auto deadline=juce::Time::getMillisecondCounterHiRes()+15000;
    while(!loaded&&juce::Time::getMillisecondCounterHiRes()<deadline)
        juce::MessageManager::getInstance()->runDispatchLoopUntil(10);
    assert(loaded);
    // SWAM publishes controller values asynchronously. Process several empty
    // blocks before readback; do not send an extra bend to make the test pass.
    auto snapshot=[&]{
        for(int i=0;i<8;++i){Access::block(*host);juce::MessageManager::getInstance()->runDispatchLoopUntil(5);}
        return bend(host->captureInstrumentState());
    };
    const auto initial=snapshot();assert(std::abs(initial)<.001);
    host->restoreEffectChain({},48000,128,[&](bool ok,const juce::String&){
        assert(ok);host->getPlugin()->setStateInformation(bent.getData(),int(bent.getSize()));
        host->resetPerformance();Access::block(*host);
    });
    const auto restored=snapshot();assert(std::abs(restored)<.001);
    {
        fengyin::ScopedGraphPause pause(&Access::graph(*host));host->resetPerformance();
        host->pitchBendChanged(-.2f);host->noteOn(60,.6f);host->breathChanged(.6f);Access::block(*host);
    }
    const auto physical=snapshot();assert(physical<-.19&&physical>-.21);
    assert(std::abs(snapshot()-physical)<.001);
    host->noteOff(60);host->pitchBendChanged(0);const auto released=snapshot();
    std::cout<<"Readback: initial="<<initial<<", restored="<<restored<<", physical="<<physical<<", released="<<released<<std::endl;
    assert(std::abs(released)<.001);
    Access::stop(*host);
    std::cout<<"Actual production cached load/paused effect restore reset="<<initial<<","<<restored
             <<"; fresh physical bend retained="<<physical<<"; release/center passed"<<std::endl;
}
void testPerformanceResetDelivery(){
    testInputStreamRecovery(); testAudioStreamRecoveryPolicy();
    using Access=fengyin::PluginHostEngineTestAccess;
    reset_delivery_test::Device device;
    for(int round=0;round<10;++round){
        auto h=std::make_unique<fengyin::PluginHostEngine>();auto& instrument=Access::install(*h,device);
        auto& player=Access::player(*h);auto& graph=Access::graph(*h);
        h->resetPerformance();Access::block(*h);assert(instrument.pitch==8192);assert(instrument.bendCount==1);
        instrument.pitch=15000;instrument.bendCount=0;instrument.received.clear();
        // Exact former failure: audio callback occurs before ScopedGraphPause
        // is destroyed by the real effect-restore completion path.
        bool complete=false;
        h->restoreEffectChain({},48000,128,[&](bool ok,const juce::String&){
            assert(ok&&graph.isSuspended());h->resetPerformance();Access::block(*h);
            assert(instrument.bendCount==0);complete=true;
        });
        assert(complete&&!graph.isSuspended());Access::block(*h);
        assert(instrument.pitch==8192&&instrument.bendCount==1);
        for(int i=0;i<8;++i)Access::block(*h);assert(instrument.bendCount==1);

        // Previous sound's queued bend/note must not override the reset. Fresh
        // physical bend, breath pair, and notes MUST survive and follow it.
        instrument.received.clear();h->pitchBendChanged(.9f);h->noteOn(65,.7f);h->resetPerformance();
        const int route=(1<<8)|3|(35<<16);
        assert(player.enqueueControllerPair(route,10000,0));h->pitchBendChanged(.25f);h->noteOn(60,.7f);
        Access::block(*h);assert(instrument.pitch==8192+juce::roundToInt(.25f*8191));
        bool freshNote=false,pair=false;int pitchBeforeNote=0;
        for(size_t i=0;i<instrument.received.size();++i){auto m=instrument.received[i];
            if(m.isPitchWheel())pitchBeforeNote=m.getPitchWheelValue();
            if(m.isNoteOn()){assert(m.getNoteNumber()==60);assert(pitchBeforeNote==instrument.pitch);freshNote=true;}
            if(m.isController()&&m.getControllerNumber()==2&&m.getControllerValue()==(10000>>7)){
                assert(i+1<instrument.received.size());auto n=instrument.received[i+1];
                assert(n.isController()&&n.getControllerNumber()==34&&n.getControllerValue()==(10000&127));pair=true;
            }
        }
        assert(freshNote&&pair);
        for(int i=0;i<8;++i)Access::block(*h);assert(instrument.pitch!=8192);

        instrument.received.clear();{
            fengyin::ScopedGraphPause pause(&graph);h->noteOff(60);h->pitchBendChanged(0);
            Access::block(*h);Access::block(*h);assert(instrument.received.empty());
        }
        Access::block(*h);assert(instrument.pitch==8192);
        assert(std::any_of(instrument.received.begin(),instrument.received.end(),[](const auto& m){return m.isNoteOff()&&m.getNoteNumber()==60;}));

        // No waiting on a plugin/UI callback lock or processor-change gate.
        for(bool gate:{false,true}){
            instrument.pitch=15000;instrument.bendCount=0;
            std::promise<void> entered,release;auto released=release.get_future();
            auto worker=std::async(std::launch::async,[&]{
                if(gate)Access::holdChangeGate(*h,entered,released);
                else {const juce::ScopedLock lock(graph.getCallbackLock());entered.set_value();released.wait();}
            });
            entered.get_future().wait();h->resetPerformance();Access::block(*h);assert(instrument.bendCount==0);
            release.set_value();worker.get();Access::block(*h);assert(instrument.pitch==8192&&instrument.bendCount==1);
        }
        // Requests and input survive a temporarily absent processor.
        player.setProcessor(nullptr);h->resetPerformance();h->noteOn(62,.5f);instrument.received.clear();
        Access::block(*h);assert(instrument.received.empty());player.setProcessor(&graph);
        juce::MessageManager::getInstance()->runDispatchLoopUntil(10);Access::block(*h);
        assert(instrument.pitch==8192);
        assert(std::any_of(instrument.received.begin(),instrument.received.end(),[](const auto& m){return m.isNoteOn()&&m.getNoteNumber()==62;}));

        // Device stop/restart must still prepare the graph and deliver queued input.
        player.audioDeviceStopped();h->resetPerformance();h->noteOn(63,.5f);instrument.received.clear();
        Access::start(*h,device);Access::block(*h);assert(instrument.pitch==8192);
        assert(std::any_of(instrument.received.begin(),instrument.received.end(),[](const auto& m){return m.isNoteOn()&&m.getNoteNumber()==63;}));

        // Latest reset wins while paused; previous-epoch data is discarded.
        instrument.received.clear();{
            fengyin::ScopedGraphPause pause(&graph);h->resetPerformance();h->pitchBendChanged(.8f);
            h->resetPerformance();h->noteOn(64,.5f);Access::block(*h);
        }
        Access::block(*h);assert(instrument.pitch==8192);

        // Generic reset CC11 policy, CH2 release and fine-CC pair unchanged.
        h->setKongExpressionMode(true);h->resetPerformance();instrument.received.clear();
        assert(player.enqueueMidi(juce::MidiMessage::noteOff(2,64),0));Access::block(*h);
        bool expression=false,ch2off=false;
        for(auto& m:instrument.received){if(m.isController()&&m.getControllerNumber()==11)expression=m.getControllerValue()==127;
            if(m.isNoteOff()&&m.getChannel()==2)ch2off=true;}
        assert(expression&&ch2off);h->setKongExpressionMode(false);

        // Overflow triggers the existing all-stop recovery; no old bent backlog
        // is replayed into the current sound. New input works after recovery.
        for(int i=0;i<8192;++i)assert(player.enqueueMidi(juce::MidiMessage::pitchWheel(1,15000),0));
        assert(!player.enqueueMidi(juce::MidiMessage::pitchWheel(1,15000),0));
        Access::block(*h);assert(instrument.pitch==8192);
        h->pitchBendChanged(-.2f);h->noteOn(67,.5f);Access::block(*h);assert(instrument.pitch<8192);
        Access::stop(*h);
    }
    // A suspended instrument must not pause the independent accompaniment mix.
    auto audioFile=juce::File::getSpecialLocation(juce::File::tempDirectory).getNonexistentChildFile("fy-reset-accompaniment", ".wav");
    {
        juce::WavAudioFormat format;std::unique_ptr<juce::AudioFormatWriter> writer(
            format.createWriterFor(audioFile.createOutputStream().release(),48000,1,16,{},0));assert(writer);
        juce::AudioBuffer<float> sound(1,48000);for(int i=0;i<48000;++i)sound.setSample(0,i,.2f);
        assert(writer->writeFromAudioSampleBuffer(sound,0,sound.getNumSamples()));
    }
    {
        fengyin::AccompanimentAudioService accompaniment;auto h=std::make_unique<fengyin::PluginHostEngine>();
        Access::install(*h,device);Access::player(*h).setAccompanimentService(&accompaniment);
        accompaniment.prepare(48000,128);assert(accompaniment.loadAudioFile(audioFile));accompaniment.play();
        juce::MessageManager::getInstance()->runDispatchLoopUntil(100);
        const auto before=accompaniment.getPosition();float peak=0;
        {fengyin::ScopedGraphPause pause(&Access::graph(*h));h->resetPerformance();
            for(int i=0;i<12;++i)peak=std::max(peak,Access::block(*h));}
        assert(peak>.05f&&accompaniment.getPosition()>before);
        Access::stop(*h);Access::player(*h).setAccompanimentService(nullptr);
    }
    assert(audioFile.deleteFile());
    std::cout<<"Accompaniment continues while instrument is suspended; device restart passed\n";
    std::cout<<"Production reset delivery: 10 rounds paused restore, queue epochs, fresh bend/note/pair, release, lock/gate, absent processor, repeated reset, Qin/CH2, overflow passed\n";
}
