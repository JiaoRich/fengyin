#include "TonePackage.h"
#include "PerformanceReverb.h"
#include "PublishedToneCatalog.h"
#include <cassert>
#include <cmath>
#include <iostream>

void testActualReverbHost(const juce::PluginDescription&,const juce::MemoryBlock&);
void testPerformanceReverbPlugin(const juce::String& path,const juce::String& outputDirectory)
{
    juce::VST3PluginFormat scanner;
    juce::OwnedArray<juce::PluginDescription> types;scanner.findAllTypesForFile(types,path);
    assert(types.size()>0);
    const auto& description=*types[0];
    assert(description.manufacturerName=="FabFilter" && description.name.containsIgnoreCase("Pro-R 2"));
    juce::AudioPluginFormatManager formats;formats.addFormat(std::make_unique<juce::VST3PluginFormat>());
    juce::String error;auto p=formats.createPluginInstance(description,48000,128,error);
    assert(p);
    auto* mix=fengyin::proR2MixParameter(*p);
    for(auto* param:p->getParameters()) if(param->getName(128).containsIgnoreCase("mix"))
        std::cout<<param->getName(128)<<" text(0,.2,1)="<<param->getText(0,64)<<","<<param->getText(.2f,64)<<","<<param->getText(1,64)<<std::endl;
    assert(mix);
    const auto out=juce::File(outputDirectory);assert(out.createDirectory().wasOk());
    p->setPlayConfigDetails(2,2,48000,128);p->prepareToPlay(48000,128);
    const auto flush=[&](){juce::AudioBuffer<float> b(2,128);juce::MidiBuffer m;b.clear();p->processBlock(b,m);};
    const auto save=[&](const char* name){flush();
        juce::MemoryBlock state;p->getStateInformation(state);assert(state.getSize()>32);
        assert(out.getChildFile(name).replaceWithData(state.getData(),state.getSize()));
    };
    const auto checkChange=[&](float value){
        std::vector<float> before;for(auto* param:p->getParameters())before.push_back(param->getValue());
        mix->setValueNotifyingHost(value);
        assert(std::abs(mix->getValue()-value)<.0001f);
        size_t i=0;for(auto* param:p->getParameters()) {
            if(param!=mix)assert(std::abs(before[i]-param->getValue())<.00001f);++i;
        }
    };
    checkChange(.2f);save("default-20.juce-state");
    const auto root=juce::File(juce::String::fromUTF8(__FILE__)).getParentDirectory().getParentDirectory();
    juce::MemoryBlock state;assert(root.getChildFile("tests/fixtures/pror2-source-mix-baseline.juce-state").loadFileAsData(state));
    p->setStateInformation(state.getData(),static_cast<int>(state.getSize()));
    save("source-before.juce-state");checkChange(.2f);save("source-20.juce-state");
    std::vector<float> saved;for(auto* param:p->getParameters())saved.push_back(param->getValue());
    juce::MemoryBlock roundtrip;p->getStateInformation(roundtrip);
    checkChange(.8f);flush();p->setStateInformation(roundtrip.getData(),static_cast<int>(roundtrip.getSize()));
    size_t index=0;for(auto* param:p->getParameters())assert(std::abs(saved[index++]-param->getValue())<.00001f);
    assert(std::abs(mix->getValue()-.2f)<.00001f);
    juce::AudioBuffer<float> buffer(2,128);juce::MidiBuffer midi;
    for(float value:{0.f,.2f,.5f,1.f,.2f}) {
        checkChange(value);double energy=0;
        for(int block=0;block<750;++block) {
            for(int c=0;c<2;++c)for(int n=0;n<128;++n)buffer.setSample(c,n,.1f*std::sin(float(block*128+n)*.04f));
            p->processBlock(buffer,midi);
            for(int c=0;c<2;++c)for(int n=0;n<128;++n){const auto x=buffer.getSample(c,n);assert(std::isfinite(x));energy+=double(x)*x;}
        }
        assert(energy>1);std::cout<<"Mix "<<value<<" energy "<<energy<<std::endl;
    }
    assert(out.getChildFile("description.xml").replaceWithText(description.createXml()->toString()));
    // Every built-in ships a settled 20% component/controller state. Load each
    // exact package in the actual plugin, not only its metadata or a fake effect.
    int count=0,maxLatency=0;
    for(const auto& tone:fengyin::publishedToneCatalog) {
        auto preset=fengyin::TonePackage::read(root.getChildFile("assets/tone-packages/"+juce::String(tone.assetName.data())),error);
        assert(preset && preset->performanceReverb && std::abs(preset->reverbMix-.2f)<.0001f);
        const auto& bytes=preset->effects.back().state;p->setStateInformation(bytes.getData(),int(bytes.getSize()));
        flush();assert(std::abs(mix->getValue()-.2f)<.0001f);maxLatency=std::max(maxLatency,p->getLatencySamples());++count;
    }
    assert(count==73);std::cout<<"73 exact package Pro-R2 states restored at 20%; maximum reported latency samples="<<maxLatency<<std::endl;
    p->releaseResources();
    testActualReverbHost(description,roundtrip);
    std::cout<<"Actual Pro-R2 production async host restore, immediate save, 0/20/80/100%, bypass and failed replacement PASS"<<std::endl;
    std::cout<<"Pro-R2 actual Mix/other-parameter preservation/full-state reload/finite processing PASS"<<std::endl;
}
