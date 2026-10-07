#include "MidiInputService.h"
#include <cassert>
#include <cmath>
#include <vector>

namespace fengyin {
struct MidiInputServiceTestAccess {
    static void send(MidiInputService& service, juce::MidiMessage message, double time) {
        message.setTimeStamp(time);
        service.handleIncomingMidiMessage(nullptr, message);
    }
};
}
struct Sink : fengyin::MidiPerformanceSink {
    struct Event { char type; float value; double time; };
    std::vector<Event> events;
    void noteOn(int, float v, double t) noexcept override { events.push_back({'N',v,t}); }
    void noteOff(int, double t) noexcept override { events.push_back({'O',0,t}); }
    void breathChanged(float v, double t) noexcept override { events.push_back({'B',v,t}); }
    void pitchBendChanged(float, double) noexcept override {}
};
int main() {
    Sink sink;
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
    service.setPerformanceSink(nullptr);
}
