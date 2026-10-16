#include <juce_audio_utils/juce_audio_utils.h>
#include "TonePackage.h"
#include "KongProjectFile.h"
#include <iostream>
#include <CoreFoundation/CoreFoundation.h>
int main(int argc,char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    if(argc!=4)return 2;
    const juce::File root(juce::String::fromUTF8(argv[1])),out(juce::String::fromUTF8(argv[2]));
    out.createDirectory();
    juce::AudioPluginFormatManager fm;fm.addFormat(std::make_unique<juce::VST3PluginFormat>());
    juce::OwnedArray<juce::PluginDescription> types;fm.getFormat(0)->findAllTypesForFile(types,juce::String::fromUTF8(argv[3]));
    if(types.isEmpty())return 3;
    struct Target {const char* id;const char* name;int index;const char* preset;};
    const Target targets[]={{"matouqin","Sus_Layers",0,""},{"nanxiao","Sus_Main",8,""},
                            {"dongxiao","Sus_Main",0,""},{"qudi","Sus_Main",1,"*Qudi1_Keyswitchs*"}};
    for(const auto& target:targets)
    {
        const auto filename="folk-"+juce::String(target.id)+".fytonepack";
        juce::String error;auto preset=fengyin::TonePackage::read(root.getChildFile(filename),error);
        if(!preset){std::cerr<<error<<std::endl;return 4;}
        auto state=fengyin::KongProjectFile::readPluginComponentState(preset->samplerState);
        const auto before=state.createCopy();auto first=state.getChildWithName("PresetList").getChild(0);
        if(*target.preset)first.setProperty("Preset",target.preset,nullptr);
        else {auto names=first.getProperty("KeyswitchNames");if(!names.isArray()||names[target.index].toString()!=target.name)return 5;}
        first.setProperty("IndexSelectedKeyswitch",target.index,nullptr);
        juce::MemoryOutputStream payload;state.writeToStream(payload);
        auto wrapper=juce::AudioProcessor::getXmlFromBinary(preset->samplerState.getData(),static_cast<int>(preset->samplerState.getSize()));
        if(!wrapper||!wrapper->getChildByName("IComponent"))return 6;
        auto* component=wrapper->getChildByName("IComponent");component->deleteAllTextElements();component->addTextElement(payload.getMemoryBlock().toBase64Encoding());
        juce::MemoryBlock changed;juce::AudioProcessor::copyXmlToBinary(*wrapper,changed);
        auto plugin=fm.createPluginInstance(*types[0],44100,128,error);if(!plugin)return 7;
        CFRunLoopRunInMode(kCFRunLoopDefaultMode,1,false);
        plugin->setStateInformation(changed.getData(),static_cast<int>(changed.getSize()));
        CFRunLoopRunInMode(kCFRunLoopDefaultMode,1,false);
        plugin->getStateInformation(preset->samplerState);
        auto actual=fengyin::KongProjectFile::readPluginComponentState(preset->samplerState);
        auto list=actual.getChildWithName("PresetList"),oldList=before.getChildWithName("PresetList");
        if(list.getNumChildren()!=oldList.getNumChildren()||!actual.getChildWithName("RackParam").isEquivalentTo(before.getChildWithName("RackParam")))return 8;
        for(int i=1;i<list.getNumChildren();++i)if(!list.getChild(i).isEquivalentTo(oldList.getChild(i)))return 9;
        first=list.getChild(0);auto names=first.getProperty("KeyswitchNames");
        if(int(first["IndexSelectedKeyswitch"])!=target.index||!names.isArray()||names[target.index].toString()!=target.name)return 10;
        for(const auto* field:{"KAI","MIDI","Audio","Volume","Tone","Balance","FXAUX"})
            if(first[field]!=oldList.getChild(0)[field])return 11;
        preset->containerProjectState=fengyin::KongProjectFile::fromPluginState(preset->samplerState,preset->containerProjectState);
        preset->pluginProgramName=first["Preset"].toString();
        if(preset->containerProjectState.isEmpty()||!fengyin::TonePackage::write(*preset,out.getChildFile(filename)))return 12;
        auto roundtrip=fengyin::TonePackage::read(out.getChildFile(filename),error);
        if(!roundtrip||roundtrip->samplerState!=preset->samplerState||roundtrip->effects.size()!=preset->effects.size())return 13;
        std::cout<<target.id<<" native default="<<target.name<<" index="<<target.index<<" slots="<<list.getNumChildren()<<" preserved other slots/rack/mix"<<std::endl;
    }
    return 0;
}
