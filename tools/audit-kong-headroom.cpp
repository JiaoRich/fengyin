#include "PluginHostEngine.h"
#include "TonePackage.h"
#include "KongProjectFile.h"
#include "MasterOutputService.h"
#include <CoreFoundation/CoreFoundation.h>
#include <iostream>
#include <set>

namespace fengyin {
struct PluginHostEngineTestAccess {
    static void prepare(PluginHostEngine& host,double rate) { host.graph->prepareToPlay(rate,128); }
    static void render(PluginHostEngine& host,juce::AudioBuffer<float>& audio) {
        juce::MidiBuffer midi;RealtimeMidiEvent event;
        while(host.player.midiQueue.pop(event))
            midi.addEvent(juce::MidiMessage(event.bytes.data(),event.size),0);
        host.graph->processBlock(audio,midi);
    }
};
}

// Offline gain audit only: no device, no new production processing, no sampler writes.
int main(int argc,char** argv) {
    juce::ScopedJuceInitialiser_GUI init;
    if(argc!=4 && argc!=5)return 2;
    const int rate=argc==5?juce::String::fromUTF8(argv[4]).getIntValue():44100;
    if(rate!=44100 && rate!=48000 && rate!=96000)return 2;
    juce::String error;
    auto preset=fengyin::TonePackage::read(juce::File(juce::String::fromUTF8(argv[2])),error);
    if(!preset || !preset->effects.empty())return 3; // Never silently omit a saved effect.
    const auto state=fengyin::KongProjectFile::readPluginComponentState(preset->samplerState);
    const auto slots=state.getChildWithName("PresetList");
    if(!state.isValid() || slots.getNumChildren()<1)return 4;
    std::set<int> notes,keys;
    bool dual=false;
    for(auto slot:slots) {
        const auto ranges=slot.getProperty("NoteRanges");
        if(!ranges.isArray() || ranges.size()<2)return 5;
        int low=127,high=0;
        for(const auto& value:*ranges.getArray()) {
            const int n=value;
            if(n<0 || n>127)return 6;
            low=juce::jmin(low,n);high=juce::jmax(high,n);
        }
        for(int n=low;n<=high;++n)notes.insert(n);
        const int keyLow=slot.getProperty("KeyswitchLo",0),keyHigh=slot.getProperty("KeyswitchHi",0);
        if(keyLow<0 || keyHigh>127 || keyHigh<keyLow)return 6;
        for(int n=keyLow;n<=keyHigh;++n)keys.insert(n);
        dual|=int(slot.getProperty("MIDI",0))==1;
    }
    for(int key:keys)notes.erase(key);
    if(notes.empty())return 7;
    const juce::File out(juce::String::fromUTF8(argv[3]));if(!out.createDirectory())return 8;
    juce::AudioPluginFormatManager fm;fm.addFormat(std::make_unique<juce::VST3PluginFormat>());
    juce::OwnedArray<juce::PluginDescription> types;
    fm.getFormat(0)->findAllTypesForFile(types,juce::String::fromUTF8(argv[1]));if(types.isEmpty())return 9;
    fengyin::PluginHostEngine host;bool done=false,success=false;
    host.loadAsync(*types[0],rate,128,preset->samplerState,[&](bool ok,const juce::String&){success=ok;done=true;});
    const auto end=juce::Time::getMillisecondCounterHiRes()+30000;
    while(!done&&juce::Time::getMillisecondCounterHiRes()<end)CFRunLoopRunInMode(kCFRunLoopDefaultMode,.01,true);
    if(!success)return 10;
    CFRunLoopRunInMode(kCFRunLoopDefaultMode,1.5,false);
    host.setKongExpressionMode(true);host.configureKongTechniqueProfile(preset->instrumentKey,preset->kongTechniques);
    fengyin::PluginHostEngineTestAccess::prepare(host,rate);
    fengyin::MasterOutputService master;master.setGain(1.5f); // maximum supported engine fader
    fengyin::ToneStyleSettings settings;
    settings.tone=preset->eqTone;settings.warmth=preset->warmth;settings.reverbMix=preset->reverbMix;
    settings.compressionThreshold=preset->compressionThreshold;settings.compressionRatio=preset->compressionRatio;
    settings.saturation=preset->saturation;settings.harshControl=preset->harshControl;
    settings.reverbRoomSize=preset->reverbRoomSize;settings.reverbDamping=preset->reverbDamping;
    settings.reverbWidth=preset->reverbWidth;settings.outputGain=preset->outputGain;
    settings.bass=preset->bass;settings.air=preset->air;
    master.setToneStyle(settings);master.setSampleRate(rate);
    const auto file=out.getChildFile("sweep-float.wav");file.deleteFile();
    std::unique_ptr<juce::OutputStream> stream=file.createOutputStream();juce::WavAudioFormat wav;
    auto options=juce::AudioFormatWriterOptions().withSampleRate(rate).withNumChannels(2).withBitsPerSample(32)
        .withSampleFormat(juce::AudioFormatWriterOptions::SampleFormat::floatingPoint);
    auto writer=wav.createWriterFor(stream,options);if(!writer)return 11;
    juce::AudioBuffer<float> audio(2,128);float peak=0,rawPeak=0;double energy=0;bool finite=true;
    juce::Array<juce::var> cases;
    const auto render=[&](float& casePeak) {
        audio.clear();fengyin::PluginHostEngineTestAccess::render(host,audio);
        for(int c=0;c<2;++c)rawPeak=juce::jmax(rawPeak,audio.getMagnitude(c,0,128));
        master.processInstrument(audio.getArrayOfWritePointers(),2,128);master.processMaster(audio.getArrayOfWritePointers(),2,128);
        for(int c=0;c<2;++c)for(int i=0;i<128;++i){const float v=audio.getSample(c,i);finite&=std::isfinite(v);casePeak=juce::jmax(casePeak,std::abs(v));energy+=double(v)*v;}
        return writer->writeFromAudioSampleBuffer(audio,0,128);
    };
    const auto run=[&](int note,int velocity,double duration) {
        host.techniqueChanged(fengyin::PerformanceTechnique::flutter,0);
        host.techniqueChanged(fengyin::PerformanceTechnique::vibrato,0);
        host.pitchBendChanged(0,0);host.breathChanged(100.f/127,0);host.noteOn(note,float(velocity)/127.f,0);
        const int blocks=int(std::ceil((duration+.35)*rate/128));
        const auto nextIt=notes.upper_bound(note);const int legato=nextIt==notes.end()?*notes.begin():*nextIt;
        bool full=false,gate=false,mod=false,bent=false,joined=false,oldOff=false,gateOff=false,off=false;
        float casePeak=0;
        for(int block=0;block<blocks;++block) {
            const double t=block*128./rate;
            if(!full&&t>=.15){host.breathChanged(1,0);full=true;}
            if(dual&&!gate&&t>=.35){host.techniqueChanged(fengyin::PerformanceTechnique::flutter,1);gate=true;}
            if(!mod&&t>=duration*.4){host.techniqueChanged(fengyin::PerformanceTechnique::vibrato,1);mod=true;}
            if(!bent&&t>=duration*.55){host.pitchBendChanged(-1,0);bent=true;}
            if(!joined&&t>=duration*.65){host.pitchBendChanged(1,0);host.noteOn(legato,float(velocity)/127.f,0);joined=true;}
            if(!oldOff&&t>=duration*.65+.05){host.noteOff(note,0);oldOff=true;}
            if(dual&&!gateOff&&t>=duration*.82){host.techniqueChanged(fengyin::PerformanceTechnique::flutter,0);gateOff=true;}
            if(!off&&t>=duration){host.noteOff(legato,0);host.breathChanged(0,0);off=true;}
            if(!render(casePeak))return false;
        }
        peak=juce::jmax(peak,casePeak);
        auto* row=new juce::DynamicObject;row->setProperty("note",note);row->setProperty("velocity",velocity);
        row->setProperty("kind","phrase");row->setProperty("seconds",duration);row->setProperty("peak",casePeak);cases.add(juce::var(row));return true;
    };
    for(int velocity:{64,95,127})for(int note:notes)if(!run(note,velocity,1.65))return 12;
    const auto middle=std::next(notes.begin(),long(notes.size()/2));
    for(int note:{*notes.begin(),*middle,*notes.rbegin()})if(!run(note,127,12))return 13;
    // Full-expression attacks matter for percussion and for a technique key
    // already held before NoteOn. Mid-phrase gating alone cannot bound these.
    for(int modulation:{0,127}) {
        if(modulation && !host.hasIndependentTechniqueRoute(fengyin::PerformanceTechnique::vibrato))continue;
        for(bool preHeld:{false,true}) {
            if(preHeld&&!dual)continue;
            for(int velocity:{64,95,127})for(int note:notes) {
                host.techniqueChanged(fengyin::PerformanceTechnique::flutter,preHeld?1.f:0.f);
                host.techniqueChanged(fengyin::PerformanceTechnique::vibrato,float(modulation)/127.f);
                host.pitchBendChanged(0,0);host.breathChanged(1,0);host.noteOn(note,float(velocity)/127.f,0);
                float casePeak=0;bool off=false;
                const int blocks=int(std::ceil(.45*rate/128));
                for(int block=0;block<blocks;++block) {
                    if(!off && block*128.0/rate>=.25) {
                        host.noteOff(note,0);host.breathChanged(0,0);
                        host.techniqueChanged(fengyin::PerformanceTechnique::flutter,0);off=true;
                    }
                    if(!render(casePeak))return 12;
                }
                peak=juce::jmax(peak,casePeak);
                auto* row=new juce::DynamicObject;row->setProperty("kind","full-expression-attack");
                row->setProperty("note",note);row->setProperty("velocity",velocity);row->setProperty("vibrato",modulation);
                row->setProperty("preHeldTechnique",preHeld);row->setProperty("peak",casePeak);cases.add(juce::var(row));
            }
        }
    }
    writer.reset();
    auto* m=new juce::DynamicObject;m->setProperty("instrument",preset->instrumentKey);
    m->setProperty("gain",preset->outputGain);m->setProperty("fader",1.5);m->setProperty("dual",dual);m->setProperty("sampleRate",rate);m->setProperty("protocol",2);
    m->setProperty("peak",peak);m->setProperty("rawPluginPeak",rawPeak);m->setProperty("finite",finite);
    m->setProperty("overloadSamples",juce::int64(master.getOverloadSamples()));
    m->setProperty("droppedMidi",juce::int64(host.getDroppedMidiEventCount()));m->setProperty("cases",cases);
    m->setProperty("scope","Native Qin + production host and saved tone processing; offline mono-note/legato matrix, both technique states, full breath, existing verified vibrato and pitch-bend extremes; no accompaniment or physical output");
    out.getChildFile("metrics.json").replaceWithText(juce::JSON::toString(juce::var(m)));
    std::cout<<preset->instrumentKey<<" cases="<<cases.size()<<" peak="<<peak<<" dropped="<<host.getDroppedMidiEventCount()<<std::endl;
    return finite&&energy>1e-6&&host.getDroppedMidiEventCount()==0?0:14;
}
