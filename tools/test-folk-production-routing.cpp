#include "PluginHostEngine.h"
#include "TonePackage.h"
#include "SimpleTechniquePolicy.h"
#include "MasterOutputService.h"
#include <CoreFoundation/CoreFoundation.h>
#include <iostream>

namespace fengyin {
struct PluginHostEngineTestAccess {
    static void prepare(PluginHostEngine& host) { host.graph->prepareToPlay(44100,128); }
    static void render(PluginHostEngine& host,juce::AudioBuffer<float>& audio,juce::OutputStream& log,int sample) {
        juce::MidiBuffer midi;RealtimeMidiEvent event;
        while(host.player.midiQueue.pop(event)) {
            juce::MidiMessage m(event.bytes.data(),event.size);
            log<<juce::String(sample)<<"\t"<<juce::String::toHexString(m.getRawData(),m.getRawDataSize())<<"\n";
            midi.addEvent(m,0);
        }
        host.graph->processBlock(audio,midi);
    }
};
}
// Exact production routing/graph, offline device, unmodified release package.
int main(int argc,char** argv) {
    juce::ScopedJuceInitialiser_GUI init;if(argc!=5)return 2;
    juce::String error;auto preset=fengyin::TonePackage::read(juce::File(juce::String::fromUTF8(argv[2])),error);
    if(!preset){std::cerr<<error;return 3;}
    const juce::String mode=juce::String::fromUTF8(argv[4]);
    const bool paced=juce::SystemStats::getEnvironmentVariable("FENGYIN_PROBE_PACED","1")!="0";
    const juce::File out(juce::String::fromUTF8(argv[3]));out.createDirectory();
    juce::AudioPluginFormatManager fm;fm.addFormat(std::make_unique<juce::VST3PluginFormat>());
    juce::OwnedArray<juce::PluginDescription> types;fm.getFormat(0)->findAllTypesForFile(types,juce::String::fromUTF8(argv[1]));if(types.isEmpty())return 4;
    fengyin::PluginHostEngine host;bool done=false,success=false;
    host.loadAsync(*types[0],44100,128,preset->samplerState,[&](bool ok,const juce::String& message){success=ok;done=true;std::cout<<message<<std::endl;});
    const auto end=juce::Time::getMillisecondCounterHiRes()+30000;
    while(!done&&juce::Time::getMillisecondCounterHiRes()<end)CFRunLoopRunInMode(kCFRunLoopDefaultMode,.01,true);
    if(!success)return 5;
    CFRunLoopRunInMode(kCFRunLoopDefaultMode,1.5,false);
    host.setKongExpressionMode(true);host.configureKongTechniqueProfile(preset->instrumentKey,preset->kongTechniques);
    fengyin::PluginHostEngineTestAccess::prepare(host);
    CFRunLoopRunInMode(kCFRunLoopDefaultMode,.1,false);
    fengyin::MasterOutputService master;master.setSampleRate(44100);master.setGain(.8f);
    fengyin::ToneStyleSettings settings;
    settings.tone=preset->eqTone;settings.warmth=preset->warmth;settings.reverbMix=preset->reverbMix;
    settings.compressionThreshold=preset->compressionThreshold;settings.compressionRatio=preset->compressionRatio;
    settings.saturation=preset->saturation;settings.harshControl=preset->harshControl;
    settings.reverbRoomSize=preset->reverbRoomSize;settings.reverbDamping=preset->reverbDamping;
    settings.reverbWidth=preset->reverbWidth;settings.outputGain=preset->outputGain;
    settings.bass=preset->bass;settings.air=preset->air;master.setToneStyle(settings);
    out.getChildFile("performance.wav").deleteFile();
    out.getChildFile("source-events.tsv").deleteFile();
    auto stream=out.getChildFile("performance.wav").createOutputStream();juce::WavAudioFormat wav;
    std::unique_ptr<juce::AudioFormatWriter> writer(wav.createWriterFor(stream.get(),44100,2,24,{},0));if(!writer)return 6;stream.release();
    auto log=out.getChildFile("source-events.tsv").createOutputStream();*log<<"sample\thex\n";
    juce::AudioBuffer<float> audio(2,128);const auto start=juce::Time::getMillisecondCounterHiRes();
    bool note=false,edge=false,off=false;int nextEdge=0;double energy=0;float peak=0;bool finite=true;
    for(int block=0;block<6891;++block){const int sample=block*128;const double t=sample/44100.;audio.clear();
        if(!note&&t>=.25){host.breathChanged(100.f/127,0);host.noteOn(67,95.f/127,0);note=true;}
        if(mode!="baseline"&&nextEdge<6&&t>=1.25+nextEdge*2.5){edge=!edge;
            if(mode=="vibrato")host.techniqueChanged(fengyin::PerformanceTechnique::vibrato,edge?100.f/127:0);
            else if(mode=="other")host.techniqueChanged(fengyin::simpleOther(preset->instrumentKey.toStdString()),edge?1:0);
            ++nextEdge;
        }
        if(!off&&t>=17){host.noteOff(67,0);host.breathChanged(0,0);off=true;}
        fengyin::PluginHostEngineTestAccess::render(host,audio,*log,sample);
        master.processInstrument(audio.getArrayOfWritePointers(),audio.getNumChannels(),audio.getNumSamples());
        master.processMaster(audio.getArrayOfWritePointers(),audio.getNumChannels(),audio.getNumSamples());
        for(int c=0;c<audio.getNumChannels();++c)for(int i=0;i<audio.getNumSamples();++i){const auto v=audio.getSample(c,i);finite=finite&&std::isfinite(v);peak=juce::jmax(peak,std::abs(v));energy+=double(v)*v;}
        writer->writeFromAudioSampleBuffer(audio,0,128);
        if(paced)while(juce::Time::getMillisecondCounterHiRes()<start+(block+1)*128000./44100)CFRunLoopRunInMode(kCFRunLoopDefaultMode,.001,false);
    }
    std::cout<<(energy>1e-6&&finite?"PASS":"FAIL audio")<<" completed production route "<<preset->instrumentKey<<" "<<mode<<" dropped="<<host.getDroppedMidiEventCount()<<" energy="<<energy<<" peak="<<peak<<" finite="<<finite<<std::endl;
    juce::DynamicObject::Ptr metrics=new juce::DynamicObject;
    metrics->setProperty("instrument",preset->instrumentKey);metrics->setProperty("mode",mode);
    metrics->setProperty("peak",peak);metrics->setProperty("finite",finite);
    metrics->setProperty("paced",paced);
    metrics->setProperty("overloadSamples",static_cast<juce::int64>(master.getOverloadSamples()));
    metrics->setProperty("droppedMidi",static_cast<juce::int64>(host.getDroppedMidiEventCount()));
    metrics->setProperty("scope","Local Qin plug-in + production host routing + saved FengYin style, instrument fader 0.8; no physical output/Windows acceptance");
    out.getChildFile("metrics.json").replaceWithText(juce::JSON::toString(juce::var(metrics.get())));
    return host.getDroppedMidiEventCount()||energy<=1e-6||!finite?7:0;
}
