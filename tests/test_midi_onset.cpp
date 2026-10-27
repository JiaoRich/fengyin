#include "MidiInputService.h"
#include "KensaVibratoRange.h"
#include <cassert>
#include <cmath>
#include <vector>
#include <iostream>
#include "RealtimeMidiQueue.h"
#include "OrderedMidiQueue.h"
#include "MidiOnsetTrace.h"
#include "SimpleTechniquePolicy.h"
#include <thread>

namespace fengyin {
struct MidiInputServiceTestAccess {
    static void seedDeviceCache(MidiInputService& service) { service.availableDeviceCache={{"test-only stale USB", "test-only-stale-usb"}}; }
    static void useTestSettings(MidiInputService& service, const juce::File& folder) {
        juce::PropertiesFile::Options options;
        options.applicationName="ReconnectTests";options.filenameSuffix=".settings";
        options.folderName=folder.getFullPathName();options.osxLibrarySubFolder="Application Support";
        service.properties.setStorageParameters(options);
        assert(service.properties.getUserSettings()->getFile().getParentDirectory()==folder);
    }
    static void restoreDevice(MidiInputService& service, const juce::String& identifier) {
        service.connectedIdentifier=identifier;
        service.activeProfile=DeviceProfileMatcher::match("");
        service.restoreBreathController();service.loadTechniqueMappings();
    }
    static void wheelWhileMappingBusy(MidiInputService& service) {
        const juce::SpinLock::ScopedLockType lock(service.techniqueLock);
        service.handleIncomingMidiMessage(nullptr, juce::MidiMessage::pitchWheel(1, 16383));
    }
    static void send(MidiInputService& service, juce::MidiMessage message, double time) {
        message.setTimeStamp(time);
        service.handleIncomingMidiMessage(nullptr, message);
    }
    static void elapsed(MidiInputService& service,double ms) {service.latestNoteStartedMs.store(juce::Time::getMillisecondCounterHiRes()-ms);}
    static void sendWhileMappingBusy(MidiInputService& service,juce::MidiMessage message) {
        const juce::SpinLock::ScopedLockType lock(service.techniqueLock);
        service.handleIncomingMidiMessage(nullptr,message);
    }
};
}
struct Sink : fengyin::MidiPerformanceSink {
    struct Event { char type; float value; double time; };
    std::vector<Event> events;
    std::vector<int> onNotes, offNotes;
    void noteOn(int n, float v, double t) noexcept override { onNotes.push_back(n); events.push_back({'N',v,t}); }
    void noteOff(int n, double t) noexcept override { offNotes.push_back(n); events.push_back({'O',0,t}); }
    void breathChanged(float v, double t) noexcept override { events.push_back({'B',v,t}); }
    void pitchBendChanged(float v, double t) noexcept override { events.push_back({'P',v,t}); }
    void techniqueChanged(fengyin::PerformanceTechnique, float v) noexcept override { events.push_back({'T',v,0}); }
};
int main() {
    {
        fengyin::MidiInputService input(false); Sink sink;
        input.setPerformanceSink(&sink); input.suspendAutomaticConnection(true);
        input.attachExternal("ble:stable-id", "Bluetooth controller");
        input.setBreathController(11);
        assert(input.getSnapshot().deviceConnected && input.isExternalConnected());
        const auto generation=input.getConnectionGeneration();
        input.acceptExternalMessage(juce::MidiMessage::controllerEvent(1,11,100));
        input.acceptExternalMessage(juce::MidiMessage::noteOn(1,64,juce::uint8(100)));
        assert(input.getSnapshot().lastNote==64 && input.getSnapshot().breath>.7f);
        fengyin::MidiInputServiceTestAccess::seedDeviceCache(input);
        input.pollConnection(); // Refresh USB choices without replacing an open external input.
        for(const auto& device:input.getLastAvailableDevices())assert(device.identifier!="test-only-stale-usb");
        assert(input.getConnectionGeneration()==generation && input.isExternalConnected());
        input.disconnect();
        assert(!input.getSnapshot().deviceConnected && input.getSnapshot().lastNote==-1);
        assert(input.getSnapshot().breath==0 && input.getConnectionGeneration()!=generation);
        input.acceptExternalMessage(juce::MidiMessage::noteOn(1,65,juce::uint8(100)));
        assert(input.getSnapshot().lastNote==-1);
        input.pollConnection();
        assert(!input.getSnapshot().deviceConnected); // Manual disconnect stays disconnected.
        input.attachExternal("ble:stable-id", "Bluetooth controller");
        assert(input.getConnectionGeneration()!=generation);
        input.setPerformanceSink(nullptr);
    }
    // Real persisted preferences, isolated from the user's settings. Reconnection
    // restores breath/mouthpiece independently; motion must never replace breath.
    const auto reconnectSettings=juce::File::getSpecialLocation(juce::File::tempDirectory)
        .getNonexistentChildFile("fengyin-reconnect-tests","",false);
    assert(reconnectSettings.createDirectory().wasOk());
    {
        fengyin::MidiInputService input(false);
        fengyin::MidiInputServiceTestAccess::useTestSettings(input,reconnectSettings);
        fengyin::MidiInputServiceTestAccess::restoreDevice(input,"device-a");
        input.setBreathController(11);
        fengyin::TechniqueMapping mouth;
        mouth.technique=fengyin::PerformanceTechnique::vibrato;
        mouth.mode=fengyin::TechniqueControlMode::hardware;
        mouth.sourceType=fengyin::TechniqueSourceType::controller;mouth.sourceNumber=74;
        input.setTechniqueMappings({mouth});
        for(int cycle=0;cycle<50;++cycle) {
            const auto generation=input.getConnectionGeneration();
            input.disconnect();assert(input.getConnectionGeneration()!=generation);
            fengyin::MidiInputServiceTestAccess::restoreDevice(input,"device-a");
            assert(input.getBreathController()==11 && input.hasCompletedBreathMatch());
            for(int v:{0,30,65,90,127,0})
                fengyin::MidiInputServiceTestAccess::send(input,juce::MidiMessage::controllerEvent(1,26,v),1);
            assert(input.getBreathController()==11 && input.getSnapshot().breath==0);
            fengyin::MidiInputServiceTestAccess::send(input,juce::MidiMessage::controllerEvent(1,11,100),1);
            assert(std::abs(input.getSnapshot().breath-100.f/127)<.001f);
            const auto mappings=input.getTechniqueMappings();
            assert(mappings.size()==1 && mappings[0].sourceNumber==74
                && mappings[0].mode==fengyin::TechniqueControlMode::hardware);
        }
        // A partial manual match interrupted by disconnect is discarded even
        // when the same device returns before the UI's next timer tick.
        input.beginBreathDetection();
        for(int v:{0,30,65,90})fengyin::MidiInputServiceTestAccess::send(input,juce::MidiMessage::controllerEvent(1,26,v),1);
        input.disconnect();
        fengyin::MidiInputServiceTestAccess::restoreDevice(input,"device-a");
        assert(input.finishBreathDetection(true)==-1 && input.getBreathController()==11);
        input.beginBreathDetection();
        for(int v:{0,30,65,90})fengyin::MidiInputServiceTestAccess::send(input,juce::MidiMessage::controllerEvent(1,26,v),1);
        assert(input.finishBreathDetection(false)==-1 && input.getBreathController()==11);
        // Explicit matching can still change the selected controller.
        input.beginBreathDetection();
        for(int v:{0,20,50,90,127,0})fengyin::MidiInputServiceTestAccess::send(input,juce::MidiMessage::controllerEvent(1,2,v),1);
        assert(input.finishBreathDetection(true)==2 && input.getBreathController()==2);
        assert(input.finishBreathDetection(true)==-1);
        input.setBreathController(11);
        input.disconnect();fengyin::MidiInputServiceTestAccess::restoreDevice(input,"device-b");
        assert(!input.hasCompletedBreathMatch() && input.getBreathController()==2);
        input.setBreathController(7);
        input.disconnect();fengyin::MidiInputServiceTestAccess::restoreDevice(input,"device-a");
        assert(input.hasCompletedBreathMatch() && input.getBreathController()==11);
    }
    {
        fengyin::MidiInputService restarted(false);
        fengyin::MidiInputServiceTestAccess::useTestSettings(restarted,reconnectSettings);
        fengyin::MidiInputServiceTestAccess::restoreDevice(restarted,"device-a");
        assert(restarted.getBreathController()==11 && restarted.hasCompletedBreathMatch());
        assert(restarted.getTechniqueMappings()[0].sourceNumber==74);
    }
    assert(reconnectSettings.deleteRecursively());
    std::cout << "Reconnect: 50 saved-CC restores, motion isolation, cancelled/explicit matches, device separation and restart persistence passed\n";
    assert(fengyin::kensaVibratoController(0)==0);
    assert(fengyin::kensaVibratoController(-1)==0);
    assert(fengyin::kensaVibratoController(1)==70);
    assert(fengyin::kensaVibratoRateController(4.5f)==69);
    assert(fengyin::kensaVibratoRateController(5.f)==85);
    assert(fengyin::kensaVibratoRateController(5.5f)==101);
    for(float hz:{4.5f,5.f,5.5f})assert(std::abs(fengyin::kensaNominalVibratoHz(fengyin::kensaVibratoRateController(hz))-hz)<.02);
    int previousKensa=0;
    for(int i=1;i<=10000;++i) {
        const auto cc=fengyin::kensaVibratoController(i/10000.f);
        assert(cc>=31 && cc<=70 && cc>=previousKensa);previousKensa=cc;
    }
    struct TraceLogger : juce::Logger {
        std::vector<juce::String> lines;
        void logMessage(const juce::String& s) override { lines.push_back(s); }
    } logger;
    {
        auto* old=juce::Logger::getCurrentLogger();juce::Logger::setCurrentLogger(&logger);
        fengyin::MidiOnsetTrace trace;trace.begin();
        // Idle controllers and synthetic keyswitches must not consume the
        // diagnostic budget before the user has begun a musical phrase.
        for(int i=0;i<3000;++i)trace.record(juce::MidiMessage::controllerEvent(1,2,0),1);
        trace.record(juce::MidiMessage::noteOn(1,24,static_cast<juce::uint8>(100)),1);
        trace.flush("test");assert(logger.lines.empty());
        trace.record(juce::MidiMessage::noteOn(1,60,static_cast<juce::uint8>(12)),2);
        trace.record(juce::MidiMessage::controllerEvent(1,1,100),3);
        trace.record(juce::MidiMessage::noteOff(1,60),4);trace.flush("test");
        assert(logger.lines.size()==3);juce::Logger::setCurrentLogger(old);
    }
    // Technique-generated Note On must never overtake the subsequent hardware
    // Note Off, even when the backend timestamp trails the local clock.
    {
        fengyin::OrderedMidiQueue<8> ordered;
        const std::uint8_t offOld[]{0x80,60,0}, onNew[]{0x91,60,90}, offNew[]{0x81,60,0};
        assert(ordered.push(offOld,3,10.01));
        assert(ordered.push(onNew,3,10.02));
        assert(ordered.push(offNew,3,10.00));
        fengyin::RealtimeMidiEvent e;
        assert(ordered.pop(e) && e.bytes[0]==0x80);
        assert(ordered.pop(e) && e.bytes[0]==0x91);
        assert(ordered.pop(e) && e.bytes[0]==0x81 && e.timestampSeconds==10.02);
        assert(!ordered.pop(e));
    }
    {
        fengyin::OrderedMidiQueue<4096> ordered;
        auto produce=[&](std::uint8_t channel){
            for(int i=0;i<1000;++i){const std::uint8_t bytes[]{channel,static_cast<std::uint8_t>(i%128),1};
                assert(ordered.push(bytes,3,20.0+i*.001));}
        };
        std::thread a(produce,0x90), b(produce,0x91);a.join();b.join();
        int counts[2]{};fengyin::RealtimeMidiEvent e;double time=0;
        while(ordered.pop(e)){const auto c=e.bytes[0]-0x90;assert(c<2);
            assert(e.bytes[1]==counts[c]%128);++counts[c];
            assert(e.timestampSeconds>=time);time=e.timestampSeconds;}
        assert(counts[0]==1000 && counts[1]==1000 && ordered.droppedCount()==0);
    }
    Sink sink;
    // Kensa mouthpiece CC/pressure drives the technique, never pitch bend.
    for(const auto source:{fengyin::TechniqueSourceType::controller,fengyin::TechniqueSourceType::channelPressure}) {
        Sink routed;fengyin::MidiInputService input(false);input.setPerformanceSink(&routed);
        input.setTechniqueContext("kontakt-kensa");fengyin::TechniqueMapping mapping;
        mapping.technique=fengyin::PerformanceTechnique::vibrato;mapping.mode=fengyin::TechniqueControlMode::hardware;
        mapping.sourceType=source;mapping.sourceNumber=74;input.setTechniqueMappings({mapping});routed.events.clear();
        for(int value:{0,64,127,0})fengyin::MidiInputServiceTestAccess::send(input,
            source==fengyin::TechniqueSourceType::controller?juce::MidiMessage::controllerEvent(1,74,value):juce::MidiMessage::channelPressureChange(1,value),1);
        assert(routed.events.size()==4);
        for(const auto& event:routed.events)assert(event.type=='T');
        assert(routed.events.front().value==0&&routed.events[2].value==1&&routed.events.back().value==0);
    }
    // A globally learned technique key must not become a musical note on
    // instruments that do not implement that role (e.g. the two xiao tones).
    for (const auto* key : {"kong-nanxiao", "kong-xiao"})
    for (const auto mode : {fengyin::TechniqueControlMode::hardware, fengyin::TechniqueControlMode::off,
                           fengyin::TechniqueControlMode::automatic})
    {
        Sink routed;
        fengyin::MidiInputService input(false);
        input.setPerformanceSink(&routed);
        input.setTechniqueContext(key);
        fengyin::TechniqueMapping mapping;
        mapping.technique=fengyin::PerformanceTechnique::mute;
        mapping.mode=mode;
        mapping.sourceType=fengyin::TechniqueSourceType::note;
        mapping.sourceNumber=24;
        input.setTechniqueMappings({mapping});
        const auto sendKey=[&](juce::MidiMessage m){fengyin::MidiInputServiceTestAccess::send(input,m,1);};
        sendKey(juce::MidiMessage::noteOn(1,67,juce::uint8(95)));
        sendKey(juce::MidiMessage::noteOn(1,24,juce::uint8(100)));
        sendKey(juce::MidiMessage::noteOff(1,24));
        assert(routed.onNotes==std::vector<int>({67}));
        assert(routed.offNotes.empty());
        fengyin::MidiInputServiceTestAccess::sendWhileMappingBusy(input,juce::MidiMessage::noteOn(1,24,juce::uint8(100)));
        fengyin::MidiInputServiceTestAccess::sendWhileMappingBusy(input,juce::MidiMessage::noteOff(1,24));
        assert(routed.onNotes==std::vector<int>({67}) && routed.offNotes.empty());
        sendKey(juce::MidiMessage::noteOff(1,67));
        assert(routed.offNotes==std::vector<int>({67}));
        // A normal note already held when the mapping is changed still gets
        // its real NoteOff, rather than becoming a stuck voice.
        sendKey(juce::MidiMessage::noteOn(1,65,juce::uint8(80)));
        mapping.sourceNumber=65;input.setTechniqueMappings({mapping});
        sendKey(juce::MidiMessage::noteOff(1,65));
        assert(routed.offNotes.back()==65);
    }
    {
        fengyin::MidiInputService defaults(false);
        const auto mappings=defaults.getTechniqueMappings();
        assert(mappings.size()==1 && mappings[0].technique==fengyin::PerformanceTechnique::vibrato);
        assert(mappings[0].mode==fengyin::TechniqueControlMode::breath);
    }
    {
        Sink pitchSink;
        fengyin::MidiInputService pitch(false);
        pitch.setPerformanceSink(&pitchSink);
        const auto sendPitch = [&](juce::MidiMessage m) { fengyin::MidiInputServiceTestAccess::send(pitch, m, 1); };
        sendPitch(juce::MidiMessage::noteOn(1, 63, (juce::uint8)80));
        pitch.setTargetKey(3);
        assert(pitchSink.offNotes.empty()); // Switching doesn't cut the held note.
        sendPitch(juce::MidiMessage::noteOff(1, 63));
        sendPitch(juce::MidiMessage::noteOn(1, 65, (juce::uint8)80));
        assert(pitchSink.onNotes == std::vector<int>({63, 68}));
        assert(pitchSink.offNotes == std::vector<int>({63}));
        pitch.setTargetKey(-2);
        sendPitch(juce::MidiMessage::noteOff(1, 65));
        assert(pitchSink.offNotes.back() == 68);
        sendPitch(juce::MidiMessage::noteOn(1, 65, (juce::uint8)80));
        assert(pitchSink.onNotes.back() == 63);
    }
    fengyin::MidiInputService service(false); // No user preferences read/written.
    service.setPerformanceSink(&sink);
    service.setTechniqueContext("kong-erhu");
    const auto send = [&](juce::MidiMessage m, double t) { fengyin::MidiInputServiceTestAccess::send(service,m,t); };
    const auto cc = [&](int v,double t) {send(juce::MidiMessage::controllerEvent(1,2,v),t);};
    const auto note = [&](int n,int v,double t) {send(juce::MidiMessage::noteOn(1,n,static_cast<juce::uint8>(v)),t);};
    // Each new phrase: quiet attack, rising breath while held, then second note.
    for(int phrase=0;phrase<3;++phrase) {
        sink.events.clear();double t=10.0+phrase*5;
        cc(0,t);note(60,12,t+.001);cc(100,t+.020);cc(127,t+.1);
        send(juce::MidiMessage::noteOff(1,60),t+1);note(62,96,t+1.001);
        assert(sink.events.size()==8);
        assert(sink.events[1].type=='B' && sink.events[1].value==0);
        assert(sink.events[2].type=='N' && std::abs(sink.events[2].value-12.f/127)<.001f);
        assert(sink.events[3].type=='B' && std::abs(sink.events[3].value-100.f/127)<.001f);
        assert(sink.events[4].type=='B' && sink.events[4].value==1);
        // Second note receives latest breath immediately before original velocity.
        assert(sink.events[5].type=='O');
        assert(sink.events[6].type=='B' && sink.events[6].value==1);
        assert(sink.events[7].type=='N' && std::abs(sink.events[7].value-96.f/127)<.001f);
        assert(sink.events[6].time==sink.events[7].time);
        send(juce::MidiMessage::noteOff(1,62),t+2);
    }
    sink.events.clear();
    cc(80,30);note(64,70,30.01);
    assert(sink.events.size()==3 && sink.events[1].type=='B');
    assert(std::abs(sink.events[1].value-80.f/127)<.001f);
    assert(std::abs(sink.events[2].value-70.f/127)<.001f);
    fengyin::TechniqueMapping mapping;
    mapping.technique = fengyin::PerformanceTechnique::vibrato;
    mapping.sourceType = fengyin::TechniqueSourceType::pitchWheel;
    mapping.mode = fengyin::TechniqueControlMode::hardware;
    mapping.strength = 1.0f;
    service.setTechniqueMappings({mapping});
    sink.events.clear();
    for (int cycle = 0; cycle < 100; ++cycle) {
        send(juce::MidiMessage::pitchWheel(1, 8192), 40);
        send(juce::MidiMessage::pitchWheel(1, 16383), 40.1);
        send(juce::MidiMessage::pitchWheel(1, 8192), 40.2);
        assert(sink.events[sink.events.size()-3].value == 0);
        assert(sink.events[sink.events.size()-2].value == 1);
        assert(sink.events.back().value == 0);
    }
    for (const auto& e : sink.events) assert(e.type == 'P'); // Old wheel mappings cannot steal pitch.
    sink.events.clear();
    fengyin::MidiInputServiceTestAccess::wheelWhileMappingBusy(service);
    assert(sink.events.size()==1 && sink.events[0].type=='P'); // Bend passes even during mapping contention.
    service.setTechniqueMappings({});
    sink.events.clear();
    send(juce::MidiMessage::pitchWheel(1, 16383), 41);
    assert(sink.events.size() == 1 && sink.events[0].type == 'P' && sink.events[0].value == 1);
    service.beginTechniqueLearn(fengyin::PerformanceTechnique::vibrato);
    send(juce::MidiMessage::pitchWheel(1,0),42);
    assert(!service.consumeTechniqueLearnResult().ready);
    assert(sink.events.back().type=='P' && sink.events.back().value==-1);
    service.cancelTechniqueLearn();
    service.setTechniqueContext("alto-sax");
    mapping.sourceType=fengyin::TechniqueSourceType::controller;
    mapping.sourceNumber=74;mapping.mode=fengyin::TechniqueControlMode::breath;
    service.setTechniqueMappings({mapping});
    note(70,80,43);cc(127,43.1);
    fengyin::MidiInputServiceTestAccess::elapsed(service,400);
    service.tickTechniqueAutomation();assert(sink.events.back().type=='T' && sink.events.back().value==0);
    fengyin::MidiInputServiceTestAccess::elapsed(service,525);
    service.tickTechniqueAutomation();assert(std::abs(sink.events.back().value-.5f)<.02f);
    fengyin::MidiInputServiceTestAccess::elapsed(service,1100);
    service.tickTechniqueAutomation();assert(sink.events.back().value==1);
    note(72,80,44);service.tickTechniqueAutomation();assert(sink.events.back().value==0);
    // Ending the preceding held note cannot restart/reset the new note timer.
    send(juce::MidiMessage::noteOff(1,70),44.1);
    fengyin::MidiInputServiceTestAccess::elapsed(service,1100);
    service.tickTechniqueAutomation();assert(sink.events.back().value==1);
    cc(0,44.2);service.tickTechniqueAutomation();assert(sink.events.back().value==0);
    // All three tempo choices retain the same original input-CC envelope:
    // zero during wait, half input halfway through fade, full input at its end.
    for (int mode=0;mode<3;++mode) {
        service.setMusicTempoMode(mode);
        const auto policy=fengyin::MusicTempoPolicy::forMode(mode);
        note(74+mode,80,45+mode);cc(127,45.1+mode);
        fengyin::MidiInputServiceTestAccess::elapsed(service,policy.waitMs-10);
        service.tickTechniqueAutomation();assert(sink.events.back().value==0);
        fengyin::MidiInputServiceTestAccess::elapsed(service,policy.waitMs+policy.fadeMs*.5);
        service.tickTechniqueAutomation();assert(std::abs(sink.events.back().value-.5f)<.02f);
        fengyin::MidiInputServiceTestAccess::elapsed(service,policy.waitMs+policy.fadeMs+10);
        service.tickTechniqueAutomation();assert(sink.events.back().value==1);
        cc(0,45.2+mode);service.tickTechniqueAutomation();assert(sink.events.back().value==0);
    }
    assert(fengyin::simpleGrowl("alto-sax") && !fengyin::simpleGrowl("soprano-sax"));
    assert(fengyin::simpleOther("kong-pipa")==fengyin::PerformanceTechnique::flutter);
    assert(fengyin::simpleOther("violin")==fengyin::PerformanceTechnique::tremolo);
    assert(fengyin::simpleOther("cello")==fengyin::PerformanceTechnique::tremolo);
    assert(fengyin::simpleVibrato("kong-erhu-2") && fengyin::simpleVibrato("kong-jinghu"));
    assert(!fengyin::simpleVibrato("kong-pipa"));
    for(const char* key:{"flute","piccolo","alto-flute","bass-flute","clarinet","bass-clarinet","oboe","english-horn","bassoon","contrabassoon","trumpet","piccolo-trumpet","alto-trombone","bass-trombone","horn-f","euphonium","bass-tuba"})
        assert(fengyin::simpleVibrato(key));
    assert(fengyin::simpleOther("kong-erhu-2")==fengyin::PerformanceTechnique::flutter);
    assert(fengyin::simpleOther("kong-matouqin")==fengyin::PerformanceTechnique::flutter);
    assert(fengyin::automaticVibratoFade(600)==0 && fengyin::automaticVibratoFade(1000)==1);
    service.setPerformanceSink(nullptr);
    // Exercise the real JUCE collector after FengYin's realtime queue, including
    // a breath update and Note On sharing the exact timestamp.
    fengyin::RealtimeMidiQueue<32> queue;
    juce::MidiMessageCollector collector;
    collector.reset(48000);
    const double now=juce::Time::getMillisecondCounterHiRes()*.001;
    std::vector<juce::MidiMessage> expected{
        juce::MidiMessage::controllerEvent(1,1,0),
        juce::MidiMessage::noteOn(1,60,static_cast<juce::uint8>(12)),
        juce::MidiMessage::controllerEvent(1,1,100),
        juce::MidiMessage::controllerEvent(1,1,127),
        juce::MidiMessage::noteOff(1,60),
        juce::MidiMessage::noteOn(1,62,static_cast<juce::uint8>(96))};
    for(size_t i=0;i<expected.size();++i)
        assert(queue.push(expected[i].getRawData(),3,now+(i<2?0:i*.001)));
    fengyin::RealtimeMidiEvent event;
    while(queue.pop(event)) collector.addMessageToQueue(juce::MidiMessage(event.bytes.data(),event.size,event.timestampSeconds));
    juce::Thread::sleep(15);
    juce::MidiBuffer actual;
    collector.removeNextBlockOfMessages(actual,1024);
    size_t index=0;
    for(const auto metadata:actual) {
        assert(index<expected.size());
        const auto message=metadata.getMessage();
        assert(message.getRawDataSize()==3);
        for(int byte=0;byte<3;++byte) assert(message.getRawData()[byte]==expected[index].getRawData()[byte]);
        ++index;
    }
    assert(index==expected.size() && queue.droppedCount()==0);
    std::cout << "Input service: 3 repeated phrases preserved; realtime queue + JUCE collector: all 6 events preserved in order\n";
}
