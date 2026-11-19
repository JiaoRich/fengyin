#pragma once
#include "SwamStateCompatibility.h"
#include <optional>
#include <regex>

namespace fengyin {
// A saved performance bend is not the instrument's tuning. Some SWAM versions
// ignore an idle MIDI centre until the first voice has started. Clear only this
// snapshot in a load-time copy, before the processor or controller can restore it.
// Never run on the audio thread or periodically reset a player's real bend.
struct SwamPerformanceState {
    struct Result { bool supported=false, changed=false; double previous=0; };
    static Result centreSavedBend(juce::MemoryBlock& state) {
        Result result;
        auto outer=juce::AudioProcessor::getXmlFromBinary(state.getData(),int(state.getSize()));
        if(!outer || !outer->hasTagName("VST3PluginState"))return result;
        auto* component=outer->getChildByName("IComponent");
        // Unknown controller payloads may carry a second copy of pitch. Do not
        // partially rewrite an unfamiliar state and report it as safe.
        if(outer->getNumChildElements()!=1 || !component)return result;
        juce::MemoryBlock body;
        if(!body.fromBase64Encoding(component->getAllSubText()) || body.getSize()<184)return result;
        const auto* p=static_cast<const char*>(body.getData());
        if(std::memcmp(p,"VstW",4) || juce::ByteOrder::bigEndianInt(p+4)!=8
            || std::memcmp(p+16,"CcnK",4) || std::memcmp(p+24,"FBCh",4)
            || juce::ByteOrder::bigEndianInt(p+28)!=2
            || juce::ByteOrder::bigEndianInt(p+20)!=body.getSize()-24
            || juce::ByteOrder::bigEndianInt(p+172)!=body.getSize()-176)return result;
        auto xml=SwamStateCompatibility::read(state);
        auto* program=xml?xml->getChildByName("program"):nullptr;
        auto* engine=program?program->getChildByName("soundengine"):nullptr;
        auto* params=engine?engine->getChildByName("params"):nullptr;
        juce::XmlElement* bend=nullptr;
        if(params)for(auto* e:params->getChildIterator())if(e->hasTagName("PARAM") && e->getStringAttribute("id")=="pitchBend") {
            if(bend)return result; bend=e;
        }
        if(!bend)return result;
        result.previous=bend->getDoubleAttribute("value",NAN);
        if(!std::isfinite(result.previous) || std::abs(result.previous)>1)return {};
        if(result.previous==0){result.supported=true;return result;}
        juce::MemoryBlock chunk(p+176,body.getSize()-176);
        const auto text=xmlText(chunk);
        if(!text)return {};
        // Preserve every other byte of the XML, the wrapper, and trailing data.
        const std::regex tag(R"re(<PARAM\b[^>]*\bid\s*=\s*"pitchBend"[^>]*>)re");
        const std::regex value(R"re(\bvalue\s*=\s*"([^"]*)")re");
        const std::sregex_iterator begin(text->begin(),text->end(),tag),end;
        if(begin==end || std::next(begin)!=end)return {};
        const auto node=begin->str();std::smatch match;
        if(!std::regex_search(node,match,value))return {};
        auto changed=*text;
        changed.replace(size_t(begin->position()+match.position(1)),size_t(match.length(1)),"0.0");
        chunk=replaceXmlText(chunk,changed);
        juce::MemoryBlock updated(p,176);updated.append(chunk.getData(),chunk.getSize());
        auto* bytes=static_cast<unsigned char*>(updated.getData());
        const auto put=[&](size_t offset,size_t n){for(int i=0;i<4;++i)bytes[offset+size_t(i)]=static_cast<unsigned char>(n>>(24-8*i));};
        put(20,updated.getSize()-24);put(172,chunk.getSize());
        auto outerText=xmlText(state);if(!outerText)return {};
        const auto encoded=component->getAllSubText().toStdString();
        const auto offset=outerText->find(encoded);
        if(offset==std::string::npos || outerText->find(encoded,offset+encoded.size())!=std::string::npos)return {};
        outerText->replace(offset,encoded.size(),updated.toBase64Encoding().toStdString());
        auto candidate=replaceXmlText(state,*outerText);
        auto check=SwamStateCompatibility::read(candidate);if(!check)return {};
        bend->setAttribute("value","0.0");
        if(!xml->isEquivalentTo(check.get(),true))return {};
        state=std::move(candidate);result.supported=true;result.changed=true;return result;
    }
private:
    static std::optional<std::string> xmlText(const juce::MemoryBlock& block) {
        if(block.getSize()<9)return {};
        const auto* p=static_cast<const char*>(block.getData());
        if(std::memcmp(p,"VC2!",4))return {};
        const size_t length=juce::ByteOrder::littleEndianInt(p+4);
        if(length>block.getSize()-9 || p[8+length]!=0)return {};
        return std::string(p+8,length);
    }
    static juce::MemoryBlock replaceXmlText(const juce::MemoryBlock& block,const std::string& text) {
        const auto* p=static_cast<const char*>(block.getData());
        const size_t oldLength=juce::ByteOrder::littleEndianInt(p+4);
        juce::MemoryBlock result(p,8);
        auto* bytes=static_cast<unsigned char*>(result.getData());
        for(int i=0;i<4;++i)bytes[4+i]=static_cast<unsigned char>(text.size()>>(8*i));
        result.append(text.data(),text.size());
        result.append(p+8+oldLength,block.getSize()-8-oldLength);return result;
    }
};
}
