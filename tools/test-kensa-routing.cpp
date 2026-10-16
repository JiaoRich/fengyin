#ifdef NDEBUG
#undef NDEBUG
#endif
#include "PluginHostEngine.h"
#include "MidiInputService.h"
#include <cassert>
#include <iostream>
namespace fengyin {
struct MidiInputServiceTestAccess {
    static void send(MidiInputService& input,const juce::MidiMessage& message){input.handleIncomingMidiMessage(nullptr,message);}
};
struct PluginHostEngineTestAccess {
    static std::vector<juce::MidiMessage> drain(PluginHostEngine& host){
        std::vector<juce::MidiMessage> result;RealtimeMidiEvent event;
        while(host.player.midiQueue.pop(event))result.emplace_back(event.bytes.data(),event.size);
        return result;
    }
};
}
int main(){
    juce::ScopedJuceInitialiser_GUI init;
    fengyin::PluginHostEngine host;host.configureKontaktControls(true);
    fengyin::PluginHostEngineTestAccess::drain(host);
    fengyin::MidiInputService input(false);input.setPerformanceSink(&host);input.setTechniqueContext("kontakt-kensa");
    fengyin::TechniqueMapping mapping;mapping.technique=fengyin::PerformanceTechnique::vibrato;
    mapping.mode=fengyin::TechniqueControlMode::hardware;mapping.sourceType=fengyin::TechniqueSourceType::controller;
    for(int sourceCC:{26,74}) {
    mapping.sourceNumber=sourceCC;
    input.setTechniqueMappings({mapping});fengyin::PluginHostEngineTestAccess::drain(host);
    for(int value:{64,127,0}){
        fengyin::MidiInputServiceTestAccess::send(input,juce::MidiMessage::controllerEvent(1,sourceCC,value));
        const auto messages=fengyin::PluginHostEngineTestAccess::drain(host);assert(!messages.empty());
        const int expected=value==0?0:juce::jlimit(31,96,30+juce::roundToInt(value/127.f*66.f));
        for(const auto& message:messages){assert(message.isController()&&message.getControllerNumber()==1&&message.getChannel()==1&&message.getControllerValue()==expected);}
    }
    }
    fengyin::MidiInputServiceTestAccess::send(input,juce::MidiMessage::pitchWheel(1,16383));
    const auto pitch=fengyin::PluginHostEngineTestAccess::drain(host);assert(pitch.size()==1&&pitch.front().isPitchWheel());
    std::cout<<"PASS production Kensa source CC26/74 -> CC1 safe range only; no raw CC26/74 passthrough; separate wheel remains Pitch Bend\n";
}
