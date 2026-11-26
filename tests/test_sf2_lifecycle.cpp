#include "Sf2Processor.h"
#include <cassert>
#include <iostream>
using namespace fengyin::sf2;

#if JUCE_WINDOWS
int wmain(int argc,wchar_t** argv)
#else
int main(int argc,char** argv)
#endif
{
    if(argc!=3)return 2;
    juce::ScopedJuceInitialiser_GUI init;
    #if JUCE_WINDOWS
    const auto arg=[&](int i){return juce::String(argv[i]);};
#else
    const auto arg=[&](int i){return juce::String::fromUTF8(argv[i]);};
#endif
    const juce::File single(arg(1)),multi(arg(2));
    // Repair only from a verified matching source; deduplicate without touching
    // unrelated resources. Use a private test directory, never a user's bank.
    const auto resourceTest=juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("fy-sf2-resource-"+juce::Uuid().toString());
    juce::String resourceHash;
    assert(importResource(single,resourceHash,resourceTest).wasOk());
    const auto managed=resourceFile(resourceHash,resourceTest);
    const auto originalTime=managed.getLastModificationTime();
    assert(importResource(single,resourceHash,resourceTest).wasOk() && managed.getLastModificationTime()==originalTime);
    assert(managed.replaceWithText("damaged"));
    assert(importResource(single,resourceHash,resourceTest).wasOk() && juce::SHA256(managed).toHexString()==resourceHash);
    auto unrelated=resourceTest.getChildFile("keep.txt");unrelated.replaceWithText("keep");
    juce::MemoryBlock truncated;assert(single.loadFileAsData(truncated));truncated.setSize(truncated.getSize()-17);
    const auto broken=resourceTest.getChildFile("truncated.sf2");assert(broken.replaceWithData(truncated.getData(),truncated.getSize()));
    assert(validateFile(broken).failed() && importResource(broken,resourceHash,resourceTest).failed());
    assert(unrelated.loadFileAsString()=="keep");assert(resourceTest.deleteRecursively());
    Processor p;p.prepareToPlay(48000,128);
    juce::AudioBuffer<float> audio(2,128);juce::MidiBuffer midi;
    auto render=[&](int blocks=1){double energy=0;for(int i=0;i<blocks;++i){p.processBlock(audio,midi);
        for(int ch=0;ch<2;++ch)for(int n=0;n<128;++n){auto v=audio.getSample(ch,n);assert(std::isfinite(v));energy+=v*v;}}
        return energy;};
    auto send=[&](juce::MidiMessage message){midi.addEvent(message,0);render();};
    auto wait=[&](auto condition){auto deadline=juce::Time::getMillisecondCounterHiRes()+12000;
        while(!condition() && juce::Time::getMillisecondCounterHiRes()<deadline){render();juce::MessageManager::getInstance()->runDispatchLoopUntil(5);}
        if(!condition())std::cerr<<p.statusText()<<std::endl;assert(condition());};
    p.chooseFile(single);wait([&]{return p.snapshot().ready;});auto original=p.snapshot();
    send(juce::MidiMessage::controllerEvent(1,11,100));send(juce::MidiMessage::noteOn(1,69,juce::uint8(90)));
    assert(render(200)>0);
    p.chooseFile(multi);wait([&]{return p.availablePresets().isArray()&&p.availablePresets().getArray()->size()==2;});
    // Choosing a multi-preset file must not silently pick one or let the
    // tuner save the old voice under the new filename.
    wait([&]{return p.statusText().contains(juce::String::fromUTF8("请选择"));});
    assert(!p.snapshot().ready && render(200)>0);
    p.cancelLoad();assert(p.snapshot().ready && p.snapshot().hash==original.hash);
    assert(p.availablePresets().getArray()->size()==1);
    p.chooseFile(multi);wait([&]{return p.statusText().contains(juce::String::fromUTF8("请选择"));});
    p.choosePreset(0,1);wait([&]{return p.snapshot().prepared;});
    assert(!p.snapshot().ready && render(200)>0); // held phrase stays audible
    send(juce::MidiMessage::controllerEvent(1,11,0));send(juce::MidiMessage::noteOff(1,69));
    wait([&]{return p.snapshot().ready;});assert(p.snapshot().program==1 && p.snapshot().hash!=original.hash);
    auto saved=p.snapshot();
    juce::TemporaryFile bad(".sf2");bad.getFile().replaceWithText("RIFF broken");
    p.chooseFile(bad.getFile());wait([&]{return p.snapshot().error.isNotEmpty();});
    assert(p.snapshot().hash==saved.hash);p.cancelLoad();
    for(int i=0;i<30;++i){p.chooseFile(single);p.cancelLoad();render(2);}
    wait([&]{return p.snapshot().ready;});assert(p.snapshot().hash==saved.hash);
    p.prepareToPlay(96000,128);wait([&]{return p.snapshot().ready && p.snapshot().error.isEmpty();});
    // A changed sample rate/restore starts neutral, no stuck breath or bend.
    assert(render(200)<1e-8);
    auto data=p.snapshot().encode();
    Processor restored;restored.prepareToPlay(48000,128);restored.setStateInformation(data.getData(),int(data.getSize()));
    auto deadline=juce::Time::getMillisecondCounterHiRes()+12000;
    while(!restored.snapshot().ready && juce::Time::getMillisecondCounterHiRes()<deadline){restored.processBlock(audio,midi);juce::MessageManager::getInstance()->runDispatchLoopUntil(5);}
    assert(restored.snapshot().ready && restored.snapshot().hash==saved.hash && restored.snapshot().program==1);
    midi.addEvent(juce::MidiMessage::noteOn(1,69,juce::uint8(90)),0);restored.processBlock(audio,midi);
    const auto neutralPeak=audio.getMagnitude(0,128);
    std::cout<<"Neutral cold-note peak="<<neutralPeak<<std::endl;
    assert(neutralPeak < .00001f); // SF2 attenuation floor, below -100 dBFS
    const auto pictures=juce::SystemStats::getEnvironmentVariable("FENGYIN_SF2_UI_EVIDENCE",{});
    if(pictures.isNotEmpty()) {
        const juce::File folder(pictures);assert(folder.createDirectory().wasOk());
        std::unique_ptr<juce::AudioProcessorEditor> editor(p.createEditor());
        for(const auto width:{420,560}) {
            editor->setSize(width,350);
            for(auto* child:editor->getChildren())if(child->isVisible())assert(editor->getLocalBounds().contains(child->getBounds()));
            auto output=folder.getChildFile("sf2-editor-"+juce::String(width)+".png").createOutputStream();
            assert(output && juce::PNGImageFormat().writeImageToStream(editor->createComponentSnapshot(editor->getLocalBounds()),*output));
        }
    }
    std::cout<<"SF2 lifecycle: multi-preset choice, held phrase, cancel/retry, invalid resource, repeated requests, sample-rate and neutral restore passed\n";
}
