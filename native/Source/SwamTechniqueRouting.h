#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <set>
#include <cstring>
namespace fengyin {
// Standard Steinberg VstW/CcnK/FBCh envelope (VST3 SDK vst2persistence.cpp),
// containing a verified JUCE XML chunk. No guessed vendor checksum fields.
struct SwamTechniqueRouting {
    struct Result { bool supported=false, changed=false; int vibrato=-1, growl=-1, flutter=-1, volume=-1; int stage=0; };
    static juce::XmlElement* findTable(juce::XmlElement& root) {
        if(root.hasTagName("MIDIRemappingTable"))return &root;
        for(auto* child:root.getChildIterator())if(auto* found=findTable(*child))return found;
        return nullptr;
    }
    static Result prepare(juce::MemoryBlock& state,bool wantVibrato,bool wantGrowl,bool wantFlutter=false,bool shareVolume=false,bool ownTremolo=false) {
        Result result;
        auto outer=juce::AudioProcessor::getXmlFromBinary(state.getData(),static_cast<int>(state.getSize()));
        if(!outer||!outer->hasTagName("VST3PluginState"))return result;
        auto* component=outer->getChildByName("IComponent");juce::MemoryBlock body;
        if(!component||!body.fromBase64Encoding(component->getAllSubText())||body.getSize()<184)return result;
        auto* bytes=static_cast<const char*>(body.getData());
        if(std::memcmp(bytes,"VstW",4)||juce::ByteOrder::bigEndianInt(bytes+4)!=8
            ||std::memcmp(bytes+16,"CcnK",4)||std::memcmp(bytes+24,"FBCh",4)
            ||juce::ByteOrder::bigEndianInt(bytes+28)!=2)return result;
        const auto bankSize=juce::ByteOrder::bigEndianInt(bytes+20);
        const auto chunkSize=juce::ByteOrder::bigEndianInt(bytes+172);
        if(bankSize!=body.getSize()-24||chunkSize!=body.getSize()-176)return result;
        auto xml=juce::AudioProcessor::getXmlFromBinary(bytes+176,static_cast<int>(chunkSize));
        if(!xml||!xml->hasTagName("swam"))return result;
        auto* table=findTable(*xml);if(!table)return result;
        // String bow tremolo belongs exclusively to the hardware technique key.
        // A saved breath mapping would otherwise overwrite OFF (and the held
        // key's selection) on the next CC11. Keep the curve and all other routes.
        if(ownTremolo)for(auto* entry:table->getChildIterator())
            if(entry->getStringAttribute("parameterId").equalsIgnoreCase("tremoloParam")
                &&entry->getIntAttribute("enabled")!=0){
                entry->setAttribute("enabled",0);result.changed=true;
            }
        juce::XmlElement* expression=nullptr;
        if(shareVolume){
            for(auto* entry:table->getChildIterator())if(entry->getStringAttribute("parameterId").equalsIgnoreCase("expression")){
                if(expression)return result;expression=entry;
            }
            if(!expression || expression->getIntAttribute("enabled")!=1 || expression->getIntAttribute("messageType",-1)!=1
                || expression->getIntAttribute("msb",-1)<0 || expression->getIntAttribute("msb",-1)>119)return result;
        }
        std::set<int> used;
        for(auto* entry:table->getChildIterator()){
            const auto id=entry->getStringAttribute("parameterId");
            if((wantVibrato&&id.equalsIgnoreCase("vibratoDepth"))||(wantGrowl&&id.equalsIgnoreCase("growl"))||(wantFlutter&&id.equalsIgnoreCase("flutter")))continue;
            if(entry->getIntAttribute("enabled")==1&&entry->getIntAttribute("messageType",-1)==1)used.insert(entry->getIntAttribute("msb",-1));
        }
        const auto configure=[&](const char* id,int preferred,bool wanted,int& selected,bool shared=false)->bool {
            juce::XmlElement* target=nullptr;
            for(auto* entry:table->getChildIterator())if(entry->getStringAttribute("parameterId").equalsIgnoreCase(id)){
                if(target)return false;target=entry;
            }
            if (!wanted) {
                if (target && target->getIntAttribute("enabled") != 0) {
                    target->setAttribute("enabled",0); result.changed=true;
                }
                return true;
            }
            if(!target){
                target=table->createNewChildElement("MIDIRemappingEntry");target->setAttribute("parameterId",id);
                auto* curve=target->createNewChildElement("MIDIRemappingCurve");
                curve->setAttribute("input_min",0);curve->setAttribute("input_max",127);
                curve->setAttribute("out_min",0);curve->setAttribute("out_max",127);
                curve->setAttribute("shape",0.0);curve->setAttribute("symmetry",0.5);
                curve->setAttribute("bypass",0);curve->setAttribute("bipolar",0);
                result.changed=true;
            }
            const auto free=[&](int cc){return cc>=1&&cc<120&&!used.count(cc);};
            selected=shared?expression->getIntAttribute("msb"):target->getIntAttribute("msb",-1);
            if(!shared && !free(selected)){selected=free(preferred)?preferred:-1;for(int i=1;selected<0&&i<120;++i)if(free(i))selected=i;}
            if(selected<0)return false;
            const auto set=[&](const char* name,int value){if(target->getIntAttribute(name,-999)!=value){target->setAttribute(name,value);result.changed=true;}};
            set("enabled",1);set("messageType",1);set("msb",selected);
            if(target->getIntAttribute("lsb",-1)>0)set("lsb",-1);
            const int channel=target->getIntAttribute("channel",-1);if(channel<1||channel>17)set("channel",17);
            if(shared){set("channel",expression->getIntAttribute("channel",17));set("lsb",expression->getIntAttribute("lsb",-1));}
            used.insert(selected);return true;
        };
        if(!configure("vibratoDepth",1,wantVibrato,result.vibrato)||!configure("growl",21,wantGrowl,result.growl)||!configure("flutter",22,wantFlutter,result.flutter)||!configure("mainVolume",0,shareVolume,result.volume,true))return Result{};
        result.supported=true;if(!result.changed)return result;
        juce::MemoryBlock chunk;juce::AudioProcessor::copyXmlToBinary(*xml,chunk);
        const auto oldXmlSize=juce::ByteOrder::littleEndianInt(bytes+180);
        const auto tail=static_cast<size_t>(8)+oldXmlSize+1;if(tail>chunkSize)return Result{};
        chunk.append(bytes+176+tail,chunkSize-tail);
        juce::MemoryBlock updated(bytes,176);updated.append(chunk.getData(),chunk.getSize());
        auto* p=static_cast<unsigned char*>(updated.getData());
        const auto put=[&](size_t offset,size_t value){for(int n=0;n<4;++n)p[offset+static_cast<size_t>(n)]=static_cast<unsigned char>(value>>(24-8*n));};
        put(20,updated.getSize()-24);put(172,chunk.getSize());
        component->deleteAllTextElements();component->addTextElement(updated.toBase64Encoding());
        juce::AudioProcessor::copyXmlToBinary(*outer,state);return result;
    }
};
}
