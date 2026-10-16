#include <juce_audio_utils/juce_audio_utils.h>
#include "KongProjectFile.h"
#include "KongPerformancePolicy.h"
#include "SuonaDualVoiceRouter.h"
#include <CoreFoundation/CoreFoundation.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <vector>

// Local native-plugin experiment. No audio device, driver or source preset writes.
struct Event {int sample,kind,note,velocity;}; // 0 off, 1 on, 2 technique off, 3 on
int main(int argc,char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;if(argc!=7)return 2;
    const juce::String mode=juce::String::fromUTF8(argv[4]);
    const int seconds=juce::String(argv[5]).getIntValue();const bool paced=juce::String(argv[6])=="paced";
    if(seconds<30||seconds>900||(mode!="crossfade"&&mode!="restart"&&mode!="baseline"&&mode!="production"))return 3;
    const juce::File out(juce::String::fromUTF8(argv[2]));out.createDirectory();
    juce::AudioPluginFormatManager fm;fm.addFormat(std::make_unique<juce::VST3PluginFormat>());
    juce::OwnedArray<juce::PluginDescription> types;fm.getFormat(0)->findAllTypesForFile(types,juce::String::fromUTF8(argv[1]));if(types.isEmpty())return 4;
    juce::String error;auto plugin=fm.createPluginInstance(*types[0],44100,128,error);if(!plugin)return 5;
    CFRunLoopRunInMode(kCFRunLoopDefaultMode,1,false);
    juce::MemoryBlock state;if(!juce::File(juce::String::fromUTF8(argv[3])).loadFileAsData(state))return 6;
    plugin->setStateInformation(state.getData(),static_cast<int>(state.getSize()));
    CFRunLoopRunInMode(kCFRunLoopDefaultMode,1.5,false);
    juce::MemoryBlock actual;plugin->getStateInformation(actual);
    auto slots=fengyin::KongProjectFile::readPluginComponentState(actual).getChildWithName("PresetList");
    if(slots.getNumChildren()!=2||slots.getChild(0)["Preset"].toString()!="Suona2_Main_Layers"
       ||slots.getChild(1)["Preset"].toString()!="Suona2_Flutter"
       ||int(slots.getChild(0)["MIDI"])!=0||int(slots.getChild(1)["MIDI"])!=1)return 7;
    std::vector<Event> events;
    const auto addNote=[&](double t,double duration,int note,int velocity){if(t>=seconds-5)return;
        events.push_back({juce::roundToInt(t*44100),1,note,velocity});
        events.push_back({juce::roundToInt(std::min(t+duration,double(seconds-5))*44100),0,note,0});};
    const std::array<int,16> melody{{60,62,64,67,69,67,64,62,60,64,67,72,74,72,69,67}};
    for(int cycle=0;cycle*180<seconds-5;++cycle){const double base=cycle*180.0;
        double t=1;int index=0;
        while(t<29){const double beat=(index%5==4?1.2:index%3==0?.8:.55);addNote(base+t,beat+(index%4==3?-.12:.012),melody[static_cast<size_t>(index%16)],70+(index%5)*8);t+=beat;++index;}
        for(int i=0;30+i*.33<54.7;++i)addNote(base+30+i*.33,.17,melody[static_cast<size_t>((i+4)%16)],75+(i%4)*8);
        for(int i=0;i<3;++i)addNote(base+56+i*10,8,60+i*7,95);
        addNote(base+86,4,67,88);addNote(base+90.7,7.3,67,88);addNote(base+100,8,74,96);
        for(int i=0;110+i*.15<139.7;++i)addNote(base+110+i*.15,.162,melody[static_cast<size_t>(i%16)],80+(i%3)*7);
        const std::array<int,6> range{{55,60,67,74,81,88}};
        for(int i=0;i<6;++i)addNote(base+140+i*4,3.5,range[static_cast<size_t>(i)],82+i*3);
        addNote(base+165,9.8,67,95);
        for(double on=3;on<174;on+=4){if((on>=120&&on<140)||on>=165)continue;
            if(base+on<seconds-5){events.push_back({juce::roundToInt((base+on)*44100),3,0,0});events.push_back({juce::roundToInt(std::min(base+on+1.2,double(seconds-5))*44100),2,0,0});}}
        for(const auto section:{std::pair<double,double>{120,139},std::pair<double,double>{165.3,174.3}})
            for(double on=section.first;on<section.second;on+=.5)if(base+on<seconds-5){
                events.push_back({juce::roundToInt((base+on)*44100),3,0,0});events.push_back({juce::roundToInt(std::min(base+on+.22,double(seconds-5))*44100),2,0,0});}
    }
    std::stable_sort(events.begin(),events.end(),[](const auto& a,const auto& b){return a.sample!=b.sample?a.sample<b.sample:a.kind<b.kind;});
    auto stream=out.getChildFile("performance.wav").createOutputStream();juce::WavAudioFormat wav;
    std::unique_ptr<juce::AudioFormatWriter> writer(wav.createWriterFor(stream.get(),44100,2,24,{},0));if(!writer)return 8;stream.release();
    auto midiLog=out.getChildFile("events.tsv").createOutputStream();*midiLog<<"sample\thex\tcause\n";
    auto sections=out.getChildFile("sections.txt").createOutputStream();*sections<<"0-30 melody/legato;30-55 staccato;56-84 long notes;86-108 breath rests/re-onset;110-140 fast legato;140-164 range/dynamics;165-175 repeated flutter;last 2s CC-only stuck-note check.\n";
    std::array<bool,128> held{};std::array<int,128> velocities{};int active=1;
    fengyin::SuonaDualVoiceRouter productionRouter;productionRouter.configure(mode=="production");
    std::array<std::array<int,128>,2> lastCC{};for(auto& row:lastCC)row.fill(-1);
    std::array<int,2> lastBend{{-1,-1}};int breath=0,bend=8192,mod=0;
    double mix=0,from=0,target=0,fadeStart=-1;
    size_t next=0;int techniqueEdges=0,techniqueNoteOns=0,sourceNoteOns=0,clipped=0;
    double peak=0,energy=0,tailEnergy=0,processSum=0;long long tailSamples=0;std::vector<double> times;
    juce::AudioBuffer<float> audio(2,128);plugin->setPlayConfigDetails(0,2,44100,128);plugin->prepareToPlay(44100,128);
    const double started=juce::Time::getMillisecondCounterHiRes();
    const int blocks=juce::roundToInt(seconds*44100.0/128);
    for(int block=0;block<blocks;++block){const int pos=block*128;const double t=pos/44100.0,local=std::fmod(t,180.0);
        audio.clear();juce::MidiBuffer midi;
        const auto send=[&](const juce::MidiMessage& m,int offset,const char* cause){midi.addEvent(m,offset);*midiLog<<juce::String(pos+offset)<<"\t"<<juce::String::toHexString(m.getRawData(),m.getRawDataSize())<<"\t"<<cause<<"\n";if(m.isNoteOn()&&juce::String(cause)=="technique")++techniqueNoteOns;};
        const auto setCC=[&](int channel,int cc,int value,int offset){auto& previous=lastCC[static_cast<size_t>(channel-1)][static_cast<size_t>(cc)];if(previous!=value){send(juce::MidiMessage::controllerEvent(channel,cc,value),offset,"controller");previous=value;}};
        if(fadeStart>=0)mix=from+(target-from)*std::clamp((t-fadeStart)/.04,0.0,1.0);
        bool any=false;for(bool on:held)any|=on;
        breath=any?juce::jlimit(20,120,juce::roundToInt(78+25*std::sin(t*.9)+12*std::sin(t*2.7))):0;
        if(t>=seconds-2)breath=100; // No NoteOn: expose hidden stuck voices after the tail.
        bend=(local>=46&&local<52)||(local>=111&&local<116)?8192+juce::roundToInt(2048*std::sin(t*3)):8192;
        mod=local>=145&&local<150?juce::roundToInt(35+20*std::sin(t*2)):0;
        const auto controllers=[&](int offset){
            if(t>=seconds-2){setCC(1,11,100,offset);setCC(2,11,100,offset);}
            else if(mode=="crossfade") {setCC(1,11,juce::roundToInt(breath*std::sqrt(1-mix)),offset);setCC(2,11,juce::roundToInt(breath*std::sqrt(mix)),offset);}
            else setCC(active,11,breath,offset);
            for(int ch=1;ch<=(mode=="production"?1:2);++ch){setCC(ch,1,mod,offset);if(lastBend[static_cast<size_t>(ch-1)]!=bend){send(juce::MidiMessage::pitchWheel(ch,bend),offset,"controller");lastBend[static_cast<size_t>(ch-1)]=bend;}}
        };
        controllers(0);
        while(next<events.size()&&events[next].sample<pos+128){const auto event=events[next++];const int offset=std::max(0,event.sample-pos);
            if(event.kind==1){held[static_cast<size_t>(event.note)]=true;velocities[static_cast<size_t>(event.note)]=event.velocity;++sourceNoteOns;
                breath=juce::jlimit(20,120,juce::roundToInt(78+25*std::sin(t*.9)+12*std::sin(t*2.7)));controllers(offset);
                send(juce::MidiMessage::noteOn(active,event.note,juce::uint8(event.velocity)),offset,"source");
                if(mode=="crossfade")send(juce::MidiMessage::noteOn(2,event.note,juce::uint8(event.velocity)),offset,"source");
            }else if(event.kind==0){held[static_cast<size_t>(event.note)]=false;send(juce::MidiMessage::noteOff(active,event.note),offset,"source");if(mode=="crossfade")send(juce::MidiMessage::noteOff(2,event.note),offset,"source");
            }else{++techniqueEdges;
                if(mode=="production")productionRouter.setFlutter(event.kind==3);
                else if(mode=="crossfade"){from=mix;target=event.kind==3?1:0;fadeStart=event.sample/44100.0;}
                else if(mode=="restart"){int destination=event.kind==3?2:1;
                    fengyin::changeKongChannel(active,destination,breath,bend,[&](int n){return held[static_cast<size_t>(n)];},[&](const auto& m){send(m,offset,"technique");},[&](int n){return velocities[static_cast<size_t>(n)]/127.f;},mod);
                    active=destination;for(auto& row:lastCC)row.fill(-1);controllers(offset);}
            }
        }
        if(mode=="production")productionRouter.process(midi,128,44100);
        const double begin=juce::Time::getMillisecondCounterHiRes();plugin->processBlock(audio,midi);
        const double elapsed=juce::Time::getMillisecondCounterHiRes()-begin;times.push_back(elapsed);processSum+=elapsed;
        for(int c=0;c<2;++c)for(int i=0;i<128;++i){const double v=audio.getSample(c,i);if(!std::isfinite(v))return 9;peak=std::max(peak,std::abs(v));energy+=v*v;if(std::abs(v)>=1)++clipped;if(t>=seconds-1){tailEnergy+=v*v;++tailSamples;}}
        writer->writeFromAudioSampleBuffer(audio,0,128);
        if(paced)while(juce::Time::getMillisecondCounterHiRes()<started+(block+1)*128000./44100)CFRunLoopRunInMode(kCFRunLoopDefaultMode,.001,false);
        else if(block%256==0)CFRunLoopRunInMode(kCFRunLoopDefaultMode,.001,false);
        if(block%10336==0)std::cout<<"Rendered "<<int(t)<<"/"<<seconds<<" seconds"<<std::endl;
    }
    plugin->releaseResources();writer.reset();midiLog.reset();std::sort(times.begin(),times.end());
    auto* result=new juce::DynamicObject();result->setProperty("mode",mode);result->setProperty("audioSeconds",seconds);result->setProperty("paced",paced);
    result->setProperty("plugin",types[0]->name);result->setProperty("pluginVersion",types[0]->version);
    result->setProperty("peak",peak);result->setProperty("clippedSamples",clipped);result->setProperty("sourceNoteOns",sourceNoteOns);
    result->setProperty("techniqueEdges",techniqueEdges);result->setProperty("techniqueNoteOns",techniqueNoteOns);
    result->setProperty("processCpuBudgetPercent",processSum/(seconds*1000)*100);
    result->setProperty("processP99Ms",times[static_cast<size_t>(times.size()*.99)]);result->setProperty("processMaxMs",times.back());
    result->setProperty("blocksOverNominalBudget",static_cast<int>(std::count_if(times.begin(),times.end(),[](double ms){return ms>128000./44100;})));
    result->setProperty("tailRmsAfterCCOnlyProbe",std::sqrt(tailEnergy/std::max(1LL,tailSamples)));
    result->setProperty("scope","Local native render, no physical audio device. Timing is processBlock wall time, not hardware xrun/Windows acceptance.");
    const auto json=juce::JSON::toString(juce::var(result));out.getChildFile("metrics.json").replaceWithText(json);std::cout<<json<<std::endl;
    return mode=="crossfade"&&techniqueNoteOns!=0?10:0;
}
