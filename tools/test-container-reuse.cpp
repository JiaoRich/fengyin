#include "PluginHostEngine.h"
#include "KongProjectFile.h"
#include <CoreFoundation/CoreFoundation.h>
#include <iostream>
int main(int argc,char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;if(argc!=3)return 2;
    juce::AudioPluginFormatManager fm;fm.addFormat(std::make_unique<juce::VST3PluginFormat>());
    juce::OwnedArray<juce::PluginDescription> types;fm.getFormat(0)->findAllTypesForFile(types,juce::String::fromUTF8(argv[1]));if(types.isEmpty())return 3;
    juce::MemoryBlock state;if(!juce::File(juce::String::fromUTF8(argv[2])).loadFileAsData(state))return 4;
    fengyin::PluginHostEngine host;juce::Array<juce::PluginDescription> descriptions;descriptions.add(*types[0]);
    host.prewarmContainers(descriptions,44100,128);
    const auto wait=[&](auto ready){const auto end=juce::Time::getMillisecondCounterHiRes()+20000;while(!ready()&&juce::Time::getMillisecondCounterHiRes()<end)CFRunLoopRunInMode(kCFRunLoopDefaultMode,.02,true);return ready();};
    if(!wait([&]{return host.warmingContainerCount()==0&&host.idleContainerCount()==1;}))return 5;
    std::cout<<"Prewarm ready; no editor/audio device attached"<<std::endl;
    juce::AudioPluginInstance* first=nullptr;
    for(int i=0;i<3;++i)
    {
        bool done=false,success=false;
        host.loadAsync(*types[0],44100,128,state,[&](bool ok,const juce::String&){success=ok;done=true;});
        if(!wait([&]{return done;})||!success)return 6;
        if(i==0)first=host.getPlugin();else if(first!=host.getPlugin())return 7;
        if(host.getPlugin()->isSuspended())return 8;
        auto* plugin=host.getPlugin();plugin->setPlayConfigDetails(0,2,44100,128);plugin->prepareToPlay(44100,128);
        double energy=0;juce::AudioBuffer<float> audio(2,128);
        for(int block=0;block<512;++block){audio.clear();juce::MidiBuffer messages;
            if(block==0){messages.addEvent(juce::MidiMessage::controllerEvent(1,11,100),0);messages.addEvent(juce::MidiMessage::controllerEvent(1,1,0),0);}
            if(block==8)messages.addEvent(juce::MidiMessage::noteOn(1,67,juce::uint8(95)),0);
            if(block==450)messages.addEvent(juce::MidiMessage::noteOff(1,67),0);
            plugin->processBlock(audio,messages);for(int c=0;c<2;++c)for(int n=0;n<128;++n)energy+=double(audio.getSample(c,n))*audio.getSample(c,n);
        }
        if(energy<1e-6)return 12;
        auto captured=host.captureContainerState();const auto actual=fengyin::KongProjectFile::readPluginComponentState(captured);
        const auto expected=fengyin::KongProjectFile::readPluginComponentState(state);
        const auto a=actual.getChildWithName("PresetList"),e=expected.getChildWithName("PresetList");
        if(a.getNumChildren()!=e.getNumChildren())return 9;
        for(int slot=0;slot<a.getNumChildren();++slot)for(const auto* field:{"Preset","MIDI","IndexSelectedKeyswitch","Volume","Tone","FXAUX"})if(a.getChild(slot)[field]!=e.getChild(slot)[field])return 10;
        std::cout<<"PASS restore "<<i<<" same instance, complete slots/default/channel/mix; audio energy="<<energy<<std::endl;
    }
    host.unload();if(host.hasPlugin())return 11;
    {
        fengyin::PluginHostEngine joining;
        joining.prewarmContainers(descriptions,44100,128);
        bool done=false,success=false;
        joining.loadAsync(*types[0],44100,128,state,[&](bool ok,const juce::String&){success=ok;done=true;});
        if(!wait([&]{return done;})||!success||joining.warmingContainerCount()!=0)return 13;
        std::cout<<"PASS foreground request joins prewarm without duplicate engine"<<std::endl;
    }
    {
        fengyin::PluginHostEngine failing;auto missing=*types[0];
        missing.fileOrIdentifier="/private/tmp/fengyin-no-such-QinEngineV3.vst3";
        juce::Array<juce::PluginDescription> broken;broken.add(missing);
        failing.prewarmContainers(broken,44100,128);
        bool done=false,success=true;
        failing.loadAsync(missing,44100,128,state,[&](bool ok,const juce::String&){success=ok;done=true;});
        if(!wait([&]{return done;})||success)return 14;
        std::cout<<"PASS failed prewarm reports foreground failure instead of leaving load pending"<<std::endl;
    }
    return 0;
}
