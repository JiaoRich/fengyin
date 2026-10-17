#include "KongProjectFile.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include <iostream>

int main(int argc,char** argv) {
    if(argc!=3)return 2;
    juce::MemoryBlock bytes;
    if(!juce::File(juce::String::fromUTF8(argv[1])).loadFileAsData(bytes))return 3;
    auto tree=fengyin::KongProjectFile::readPluginComponentState(bytes);
    if(!tree.isValid())return 4;
    auto original=tree.createCopy();
    auto rack=tree.getChildWithName("RackParam"),slots=tree.getChildWithName("PresetList");
    if(!rack.isValid()||slots.getNumChildren()<1||!rack.hasProperty("Gain"))return 5;
    std::cout<<"Gain "<<rack["Gain"].toString()<<" -> 6; slots "<<slots.getNumChildren()<<'\n';
    rack.setProperty("Gain",6.0,nullptr);
    for(auto slot:slots) {
        if(!slot.hasProperty("FXAUX"))return 6;
        std::cout<<"FXAUX "<<slot["FXAUX"].toString()<<" -> 50\n";
        slot.setProperty("FXAUX",50.0,nullptr);
    }
    auto restored=tree.createCopy();
    restored.getChildWithName("RackParam").setProperty("Gain",original.getChildWithName("RackParam")["Gain"],nullptr);
    for(int i=0;i<slots.getNumChildren();++i)
        restored.getChildWithName("PresetList").getChild(i).setProperty("FXAUX",original.getChildWithName("PresetList").getChild(i)["FXAUX"],nullptr);
    if(!restored.isEquivalentTo(original))return 7;
    juce::MemoryOutputStream out;tree.writeToStream(out);auto result=out.getMemoryBlock();
    auto wrapper=juce::AudioProcessor::getXmlFromBinary(bytes.getData(),int(bytes.getSize()));
    if(wrapper&&wrapper->hasTagName("VST3PluginState")) {
        auto* component=wrapper->getChildByName("IComponent");if(!component)return 8;
        component->deleteAllTextElements();component->addTextElement(result.toBase64Encoding());
        juce::AudioProcessor::copyXmlToBinary(*wrapper,result);
    }
    auto actual=fengyin::KongProjectFile::readPluginComponentState(result);
    if(!actual.isEquivalentTo(tree))return 9;
    return juce::File(juce::String::fromUTF8(argv[2])).replaceWithData(result.getData(),result.getSize())?0:10;
}
