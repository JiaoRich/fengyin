#include <juce_audio_utils/juce_audio_utils.h>
#include "SwamStringTremolo.h"
#include "SwamStateCompatibility.h"
#include <CoreFoundation/CoreFoundation.h>
#include <iostream>
struct TempoHead:juce::AudioPlayHead {
    double bpm=100,seconds=0,ppq=0;juce::int64 samples=0;
    juce::Optional<PositionInfo> getPosition() const override {
        PositionInfo p;p.setBpm(bpm);p.setTimeInSamples(samples);p.setTimeInSeconds(seconds);p.setPpqPosition(ppq);p.setIsPlaying(true);return p;
    }
};
int main(int argc,char** argv) {
    juce::ScopedJuceInitialiser_GUI init;if(argc<3||argc>4)return 2;
    juce::AudioPluginFormatManager fm;fm.addFormat(std::make_unique<juce::VST3PluginFormat>());
    juce::OwnedArray<juce::PluginDescription> types;fm.getFormat(0)->findAllTypesForFile(types,juce::String::fromUTF8(argv[1]));if(types.isEmpty())return 3;
    juce::String error;auto plugin=fm.createPluginInstance(*types[0],44100,128,error);if(!plugin)return 4;
    CFRunLoopRunInMode(kCFRunLoopDefaultMode,1,false);
    if(argc==4){
        juce::ZipFile zip(juce::File(juce::String::fromUTF8(argv[3])));std::unique_ptr<juce::InputStream> input(zip.createStreamForEntry(zip.getIndexOfFileName("tone.xml")));
        auto document=input?juce::parseXML(input->readEntireStreamAsString()):nullptr;
        auto* preset=document?document->getChildByName("PRESET"):nullptr;auto* stored=preset?preset->getChildByName("INSTRUMENT_STATE"):nullptr;
        juce::MemoryBlock source;if(!stored||!source.fromBase64Encoding(stored->getAllSubText()))return 5;
        plugin->setStateInformation(source.getData(),static_cast<int>(source.getSize()));CFRunLoopRunInMode(kCFRunLoopDefaultMode,1,false);
        juce::MemoryBlock returned;plugin->getStateInformation(returned);
        if(!fengyin::SwamStateCompatibility::preserved(source,returned))return 6;
    }
    juce::MemoryBlock baseline;plugin->getStateInformation(baseline);
    const juce::File out(juce::String::fromUTF8(argv[2]));out.createDirectory();
    for(bool enabled:{false,true}) {
        plugin->setStateInformation(baseline.getData(),static_cast<int>(baseline.getSize()));
        fengyin::SwamStringTremolo tremolo;
        const double bpm=std::getenv("FY_TEST_BPM")?std::atof(std::getenv("FY_TEST_BPM")):100;
        tremolo.setTempoMode(bpm<80?0:bpm>120?2:1);
        if(!tremolo.prepare(*plugin))return 7;
        juce::MemoryBlock configured;plugin->getStateInformation(configured);
        auto original=fengyin::SwamStateCompatibility::read(baseline),actual=fengyin::SwamStateCompatibility::read(configured);
        if(!original||!actual)return 8;
        std::map<juce::String,double> a,b;fengyin::SwamStateCompatibility::params(*original,a);fengyin::SwamStateCompatibility::params(*actual,b);
        for(const auto& [key,value]:a)if(key!="tremoloMode"&&key!="tremoloSpeed"&&key!="tremoloParam")
            if(!b.count(key)||std::abs(value-b[key])>.0001){std::cerr<<"Unexpected parameter change "<<key;return 9;}
        if(!fengyin::SwamStateCompatibility::table(*original)->isEquivalentTo(fengyin::SwamStateCompatibility::table(*actual),true))return 10;
        TempoHead head;if(auto* value=std::getenv("FY_TEST_BPM"))head.bpm=std::atof(value);
        plugin->setPlayHead(&head);
        plugin->setPlayConfigDetails(0,2,44100,128);plugin->prepareToPlay(44100,128);
        auto stream=out.getChildFile(enabled?"tremolo.wav":"baseline.wav").createOutputStream();juce::WavAudioFormat wav;
        std::unique_ptr<juce::AudioFormatWriter> writer(wav.createWriterFor(stream.get(),44100,2,24,{},0));if(!writer)return 11;stream.release();
        juce::AudioBuffer<float> audio(2,128);double energy=0;int notes=0;const int pitch=plugin->getName().contains("Cello")?55:67;
        for(int block=0;block<3101;++block){audio.clear();juce::MidiBuffer midi;const double t=block*128./44100;
            const auto at=[&](double time){return block*128<=time*44100&&(block+1)*128>time*44100;};
            if(block==0){midi.addEvent(juce::MidiMessage::controllerEvent(1,11,100),0);midi.addEvent(juce::MidiMessage::controllerEvent(1,1,0),0);}
            if(at(.25)){midi.addEvent(juce::MidiMessage::noteOn(1,pitch,juce::uint8(95)),0);++notes;}
            if(at(2))tremolo.request(enabled);
            if(at(3.5)&&std::getenv("FY_TEST_SWITCH")){head.bpm=140;tremolo.setTempoMode(2);}
            if(at(5))tremolo.request(false);
            if(at(7.5))midi.addEvent(juce::MidiMessage::noteOff(1,pitch),0);
            tremolo.flush();plugin->processBlock(audio,midi);writer->writeFromAudioSampleBuffer(audio,0,128);
            head.samples+=128;head.seconds+=128./44100;head.ppq+=128./44100*head.bpm/60;
            for(int c=0;c<2;++c)for(int i=0;i<128;++i)energy+=double(audio.getSample(c,i))*audio.getSample(c,i);
            if(block%256==0)CFRunLoopRunInMode(kCFRunLoopDefaultMode,.001,false);
            juce::ignoreUnused(t);
        }
        if(energy<1e-6||notes!=1)return 12;
        for(auto* p:plugin->getParameters())if(p->getName(128)=="Tremolo"&&p->getText(p->getValue(),128)!="OFF")return 13;
        writer.reset();plugin->releaseResources();plugin->setPlayHead(nullptr);
        std::cout<<"PASS "<<types[0]->name<<" "<<types[0]->version<<" enabled="<<enabled<<" energy="<<energy<<" musicalNoteOns="<<notes<<" other parameters/curves unchanged; release OFF"<<std::endl;
    }
}
