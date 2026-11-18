#include "TonePackage.h"
#include "DjSourceRouting.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include <cassert>
#include <cmath>
#include <iostream>
int main() {
    juce::ScopedJuceInitialiser_GUI init;
    juce::AudioPluginFormatManager formats;formats.addFormat(std::make_unique<juce::VST3PluginFormat>());
    const auto root=juce::File(juce::String::fromUTF8(__FILE__)).getParentDirectory().getParentDirectory();
    juce::String error;
    const auto preset=fengyin::TonePackage::read(root.getChildFile("assets/tone-packages/dj-source-16.fytonepack"),error);
    assert(preset && preset->effects.size()==2);
    for(const auto& saved:preset->effects) {
        juce::PluginDescription description;const auto xml=juce::parseXML(saved.descriptionXml);
        assert(xml && description.loadFromXml(*xml));
        // Production MainComponent resolves package identity against the local scan.
        // Repeat that here: installer layouts may use either VST3 root or vendor folder.
        juce::VST3PluginFormat scanner;
        bool found=false;
        for(const auto& file:scanner.searchPathsForPlugins(scanner.getDefaultLocationsToSearch(),true,false)) {
            if(!juce::File(file).getFileName().containsIgnoreCase(description.name))continue;
            juce::OwnedArray<juce::PluginDescription> types;scanner.findAllTypesForFile(types,file);
            for(auto* candidate:types)if(candidate->uniqueId==description.uniqueId
                &&candidate->manufacturerName==description.manufacturerName) {
                std::cout<<description.name<<" required="<<description.version<<" scanned="<<candidate->version<<std::endl;
                assert(fengyin::djEffectVersionCompatible(description,*candidate));
                assert(candidate->deprecatedUid==description.deprecatedUid);
                description=*candidate;found=true;break;
            }
            if(found)break;
        }
        if(!found){std::cerr<<"Local scanned identity not found: "<<description.name<<std::endl;return 1;}
        auto plugin=formats.createPluginInstance(description,48000,128,error);
        if(!plugin){std::cerr<<description.name<<": "<<error<<std::endl;return 1;}
        std::vector<float> defaults;for(auto* p:plugin->getParameters())defaults.push_back(p->getValue());
        plugin->setStateInformation(saved.state.getData(),static_cast<int>(saved.state.getSize()));
        std::vector<float> parameters;for(auto* p:plugin->getParameters())parameters.push_back(p->getValue());
        assert(defaults.size()==parameters.size());int changed=0;
        for(size_t i=0;i<parameters.size();++i)if(std::abs(parameters[i]-defaults[i])>1e-5f)++changed;
        assert(changed>0); // Reject silently ignored source state / factory defaults.
        juce::MemoryBlock roundtrip;plugin->getStateInformation(roundtrip);assert(roundtrip.getSize()>32);
        plugin->setStateInformation(roundtrip.getData(),static_cast<int>(roundtrip.getSize()));
        size_t n=0;for(auto* p:plugin->getParameters())assert(std::abs(parameters[n++]-p->getValue())<1e-5f);
        plugin->setPlayConfigDetails(2,2,48000,128);plugin->prepareToPlay(48000,128);
        juce::AudioBuffer<float> buffer(2,128);juce::MidiBuffer midi;double energy=0;
        for(int block=0;block<750;++block) {
            for(int c=0;c<2;++c)for(int i=0;i<128;++i)buffer.setSample(c,i,.15f*std::sin(float(block*128+i)*.05f));
            plugin->processBlock(buffer,midi);
            for(int c=0;c<2;++c)for(int i=0;i<128;++i){const float v=buffer.getSample(c,i);assert(std::isfinite(v));energy+=double(v)*v;}
        }
        assert(energy>1);std::cout<<description.name<<" state="<<saved.state.getSize()<<" parameters="<<parameters.size()<<" sourceChanged="<<changed<<" latency="<<plugin->getLatencySamples()<<" energy="<<energy<<std::endl;
        plugin->releaseResources();
    }
}
