#include "MidiInputService.h"
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
    static void wheelWhileMappingBusy(MidiInputService& service) {
        const juce::SpinLock::ScopedLockType lock(service.techniqueLock);
        service.handleIncomingMidiMessage(nullptr, juce::MidiMessage::pitchWheel(1, 16383));
    }
    static void send(MidiInputService& service, juce::MidiMessage message, double time) {
        message.setTimeStamp(time);
        service.handleIncomingMidiMessage(nullptr, message);
    }
    static void elapsed(MidiInputService& service,double ms) {service.latestNoteStartedMs.store(juce::Time::getMillisecondCounterHiRes()-ms);}
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
    assert(fengyin::simpleGrowl("alto-sax") && !fengyin::simpleGrowl("soprano-sax"));
    assert(fengyin::simpleOther("kong-pipa")==fengyin::PerformanceTechnique::tremolo);
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
