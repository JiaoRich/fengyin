#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <map>
#include <cmath>
#include <cstring>
namespace fengyin {
struct SwamStateCompatibility {
    static std::unique_ptr<juce::XmlElement> read(const juce::MemoryBlock& state) {
        auto outer=juce::AudioProcessor::getXmlFromBinary(state.getData(),static_cast<int>(state.getSize()));
        auto* component=outer?outer->getChildByName("IComponent"):nullptr;juce::MemoryBlock body;
        if(!component||!body.fromBase64Encoding(component->getAllSubText())||body.getSize()<184)return {};
        const auto* p=static_cast<const char*>(body.getData());
        if(std::memcmp(p,"VstW",4)||std::memcmp(p+16,"CcnK",4)||std::memcmp(p+24,"FBCh",4))return {};
        const auto size=juce::ByteOrder::bigEndianInt(p+172);if(size!=body.getSize()-176)return {};
        auto xml=juce::AudioProcessor::getXmlFromBinary(p+176,static_cast<int>(size));
        return xml&&xml->hasTagName("swam")?std::move(xml):nullptr;
    }
    static void params(const juce::XmlElement& node,std::map<juce::String,double>& out) {
        if(node.hasTagName("PARAM"))out[node.getStringAttribute("id")]=node.getDoubleAttribute("value",NAN);
        for(auto* child:node.getChildIterator())params(*child,out);
    }
    static const juce::XmlElement* table(const juce::XmlElement& node) {
        if(node.hasTagName("MIDIRemappingTable"))return &node;
        for(auto* child:node.getChildIterator())if(auto* found=table(*child))return found;return nullptr;
    }
    static bool preserved(const juce::MemoryBlock& source,const juce::MemoryBlock& restored) {
        auto a=read(source),b=read(restored);if(!a||!b)return false;
        std::map<juce::String,double> before,after;params(*a,before);params(*b,after);
        if(before.size()<20)return false;
        for(const auto& item:before){auto found=after.find(item.first);if(found==after.end()||!std::isfinite(item.second)||!std::isfinite(found->second)||std::abs(item.second-found->second)>.0001)return false;}
        const auto* x=table(*a);const auto* y=table(*b);if(!x||!y)return false;
        return x->isEquivalentTo(y,true);
    }
};
}
