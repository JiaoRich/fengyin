#include "MasterOutputService.h"
#include <array>
#include <cassert>
#include <cmath>
#include <limits>
int main() {
    const juce::ScopedNoDenormals noDenormals;
    constexpr int frames=128;
    for(float fault:{std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::infinity(),-std::numeric_limits<float>::infinity()})
    for(int lane=0;lane<2;++lane)for(float wet:{0.f,.25f}) {
        fengyin::MasterOutputService dsp;fengyin::ToneStyleSettings style;style.air=.4f;style.reverbMix=wet;
        dsp.setToneStyle(style);dsp.setSampleRate(48000);
        std::array<float,frames> l{},r{};float* channels[]{l.data(),r.data()};
        for(int b=0;b<200;++b) {
            for(int i=0;i<frames;++i)l[i]=r[i]=.4f*std::sin(float(b*frames+i)*.057f);
            if(b<20)channels[lane][17]=fault; // repeated invalid input followed by held normal audio
            dsp.processInstrument(channels,2,frames);dsp.processMaster(channels,2,frames);
            float peak=0;for(int i=0;i<frames;++i){assert(std::isfinite(l[i])&&std::isfinite(r[i]));peak=std::max({peak,std::abs(l[i]),std::abs(r[i])});}
            assert(peak<2);if(b==199)assert(peak>.01f);
        }
        assert(dsp.getInvalidInputSamples()==20);assert(dsp.getSignalRecoveries()==0);
        l.fill(2.f);r.fill(-3.f);dsp.processMaster(channels,2,frames);
        assert(l[0]==2&&r[0]==-3); // finite over-range is not attenuated
        l[0]=fault;dsp.processMaster(channels,2,frames);assert(l[0]==0&&dsp.getInvalidOutputSamples()==1);
        dsp.setSampleRate(44100);l.fill(.1f);r.fill(.1f);dsp.processInstrument(channels,2,frames);
        assert(std::isfinite(l[0])&&std::isfinite(r[0]));
    }
    // Internal arithmetic overflow must not poison subsequent valid blocks.
    fengyin::MasterOutputService dsp;dsp.setSampleRate(48000);
    std::array<float,frames> l{},r{};float* outputs[]{l.data(),r.data()};
    for(int b=0;b<20;++b){l.fill(b%2?std::numeric_limits<float>::max():-std::numeric_limits<float>::max());r=l;dsp.processInstrument(outputs,2,frames);}
    for(int b=0;b<100;++b){l.fill(.2f);r=l;dsp.processInstrument(outputs,2,frames);for(float x:l)assert(std::isfinite(x));}
    assert(dsp.getSignalRecoveries()>0);assert(l[0]>.01f&&l[0]<1.f);
    dsp.processInstrument(nullptr,0,0);dsp.processMaster(nullptr,0,0);
}
