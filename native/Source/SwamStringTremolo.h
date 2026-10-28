#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>
#include <cmath>
#include "MusicTempoPolicy.h"
namespace fengyin {
// Native discrete bowing articulation, not a volume LFO or Vibrato.
class SwamStringTremolo {
public:
    void clear() noexcept { selector.store(nullptr); speedParameter=nullptr; modeParameter=nullptr; engaged=false; wanted.store(false); dirty.store(false); }
    void setTempoMode(int value) noexcept { if(tempoMode.exchange(MusicTempoPolicy::valid(value))!=MusicTempoPolicy::valid(value))dirty.store(true); }
    bool prepare(juce::AudioProcessor& plugin) {
        clear();
        juce::AudioProcessorParameter *trem=nullptr,*mode=nullptr,*speed=nullptr;
        for(auto* p:plugin.getParameters()) {
            const auto name=p->getName(128);
            if(name=="Tremolo")trem=p;
            else if(name=="Tremolo Mode")mode=p;
            else if(name=="Tremolo Min Speed")speed=p;
        }
        if(!trem||!mode||!speed)return false;
        const auto off=findLabel(*trem,"OFF"),slow=findLabel(*trem,"Slow"),fast=findLabel(*trem,"Fast"),hz=findLabel(*mode,"Hz");
        // Six strokes per quarter note (sixteenth-note triplets). Fast is 1.5 x Slow.
        const double rates[]{7.0,10.0,140.0/15.0};
        for(int i=0;i<3;++i){rateValues[i]=findNumber(*speed,rates[i]);if(rateValues[i]<0)return false;}
        if(off<0||slow<0||fast<0||hz<0)return false;
        offValue=off;slowValue=slow;fastValue=fast;hzValue=hz;
        speedParameter=speed;modeParameter=mode;selector.store(trem);
        // Discovery must never overwrite the tuner's saved mode, speed or articulation.
        return true;
    }
    bool available() const noexcept { return selector.load()!=nullptr; }
    void request(bool on) noexcept { if(wanted.exchange(on)!=on)dirty.store(true); }
    void flush() noexcept {
        if(dirty.exchange(false))if(auto* p=selector.load()) {
            if (wanted.load()) {
                if (!engaged) { baselineSelector=p->getValue(); baselineSpeed=speedParameter->getValue(); baselineMode=modeParameter->getValue(); engaged=true; }
                const auto tempo=tempoMode.load();
                modeParameter->setValueNotifyingHost(hzValue);
                speedParameter->setValueNotifyingHost(rateValues[tempo]);
                p->setValueNotifyingHost(tempo==2?fastValue:slowValue);
            } else if (engaged) {
                p->setValueNotifyingHost(baselineSelector);
                speedParameter->setValueNotifyingHost(baselineSpeed);
                modeParameter->setValueNotifyingHost(baselineMode);
                engaged=false;
            }
        }
    }
    template <typename Capture> void captureWithoutPerformanceOverride(Capture&& capture) {
        auto* p=selector.load();
        if (!engaged || !p) { capture(); return; }
        const auto current=p->getValue(), speed=speedParameter->getValue(), mode=modeParameter->getValue();
        p->setValueNotifyingHost(baselineSelector);speedParameter->setValueNotifyingHost(baselineSpeed);modeParameter->setValueNotifyingHost(baselineMode);
        capture();
        p->setValueNotifyingHost(current);speedParameter->setValueNotifyingHost(speed);modeParameter->setValueNotifyingHost(mode);
    }
private:
    static float findLabel(juce::AudioProcessorParameter& p,const juce::String& label) {
        const auto parsed=p.getValueForText(label);
        if(std::isfinite(parsed)&&parsed>=0&&parsed<=1&&p.getText(parsed,128).equalsIgnoreCase(label))return parsed;
        float best=-1;for(int i=0;i<=100;++i){const float v=i/100.f;
            if(p.getText(v,128).equalsIgnoreCase(label)&&(best<0||std::abs(v-.5f)<std::abs(best-.5f)))best=v;}
        return best;
    }
    static float findNumber(juce::AudioProcessorParameter& p,double value) {
        const auto parsed=p.getValueForText(juce::String(value));
        if(std::isfinite(parsed)&&parsed>=0&&parsed<=1&&std::abs(p.getText(parsed,128).getDoubleValue()-value)<.01)return parsed;
        float best=-1;double error=.05;
        for(int i=0;i<=1000;++i){const float v=i/1000.f;const double e=std::abs(p.getText(v,128).getDoubleValue()-value);
            if(e<error){error=e;best=v;}}
        return best;
    }
    std::atomic<juce::AudioProcessorParameter*> selector{nullptr};
    std::atomic<bool> wanted{false},dirty{false};
    std::atomic<int> tempoMode{1};
    juce::AudioProcessorParameter *speedParameter=nullptr,*modeParameter=nullptr;
    bool engaged=false;
    float baselineSelector=0,baselineSpeed=0,baselineMode=0,hzValue=0;
    float offValue=0,slowValue=.5f,fastValue=1,rateValues[3]{};
};
}
