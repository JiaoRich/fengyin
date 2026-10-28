#include "KongDualSlotRouter.h"
#include <cassert>
#include <vector>
int main() {
    using M=juce::MidiMessage;
    fengyin::KongDualSlotRouter router;
    std::vector<M> messages;
    int sounding[3][128]{};
    auto send=[&](const M& m){
        messages.push_back(m); const int ch=m.getChannel();
        if(ch<1||ch>2)return;
        if(m.isNoteOn()) {++sounding[ch][m.getNoteNumber()];assert(sounding[3-ch][m.getNoteNumber()]==0);}
        else if(m.isNoteOff()) sounding[ch][m.getNoteNumber()]=0;
        else if(m.isAllNotesOff()||m.isAllSoundOff())for(auto& note:sounding[ch])note=0;
    };
    auto route=[&](const M& m){router.route(m,send);};
    auto gate=[&](bool on){messages.clear();router.setTechnique(on,send);};
    router.configure(true);
    route(M::controllerEvent(1,11,103));route(M::controllerEvent(1,1,25));route(M::pitchWheel(1,10000));
    route(M::noteOn(1,60,juce::uint8(91)));
    assert(sounding[1][60]==1&&sounding[2][60]==0);
    gate(true);
    assert(messages.front().isNoteOff()&&messages.front().getChannel()==1);
    assert(messages[1].isControllerOfType(11)&&messages[1].getControllerValue()==103);
    assert(messages[2].isPitchWheel()&&messages[2].getPitchWheelValue()==10000);
    assert(messages.back().isNoteOn()&&messages.back().getChannel()==2&&messages.back().getVelocity()==91);
    gate(false);assert(sounding[1][60]==1&&sounding[2][60]==0);
    gate(false);assert(messages.empty());
    gate(true);route(M::noteOff(1,60));route(M::noteOn(1,67,juce::uint8(80)));
    assert(sounding[1][67]==0&&sounding[2][67]==1);
    gate(false);assert(sounding[1][67]==1&&sounding[2][67]==0);
    route(M::noteOff(1,67));gate(true);
    for(const auto& m:messages)assert(!m.isNoteOn());
    route(M::noteOn(1,72,juce::uint8(88)));
    messages.clear();router.reset(send);
    assert(messages.size()==1&&messages[0].isNoteOff()&&messages[0].getChannel()==2);
    gate(true);for(const auto& m:messages)assert(!m.isNoteOn());
    route(M::noteOn(1,62,juce::uint8(90)));route(M::allNotesOff(1));gate(false);
    for(const auto& m:messages)assert(!m.isNoteOn());
    route(M::noteOn(1,64,juce::uint8(76)));
    for(int i=0;i<1000;++i){gate(true);assert(sounding[2][64]==1&&sounding[1][64]==0);gate(false);assert(sounding[1][64]==1&&sounding[2][64]==0);}
    router.reset(send);assert(sounding[1][64]==0&&sounding[2][64]==0);
    router.configure(false);messages.clear();route(M::controllerEvent(1,11,50));
    assert(messages.size()==1&&messages[0].getControllerValue()==50);
}
