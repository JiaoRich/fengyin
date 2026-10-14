#pragma once
#include <array>
#include <vector>
#include <algorithm>
#include <cmath>
namespace fengyin {
// Prepare while audio is stopped. No allocation, locks or file access in process.
class WetPreDelay {
public:
    void prepare(double sampleRate) {
        rate=std::isfinite(sampleRate)&&sampleRate>0?sampleRate:48000;
        const auto size=static_cast<size_t>(std::ceil(rate*.1))+2;
        for(auto& channel:ring)channel.assign(size,0);
        rampLength=std::max(1,static_cast<int>(rate*.05));initialised=false;delay=0;reset();
    }
    void reset() noexcept {for(auto& c:ring)std::fill(c.begin(),c.end(),0);position=0;ramp=0;oldDelay=delay;}
    void setDelay(float ms) noexcept {
        if(ring[0].empty())return;
        const int requested=std::clamp(static_cast<int>(std::round((std::isfinite(ms)?ms:0)*rate/1000)),0,static_cast<int>(ring[0].size())-1);
        if(!initialised){oldDelay=delay=requested;initialised=true;return;}
        if(requested==delay)return;
        oldDelay=ramp>0&&ramp>rampLength/2?oldDelay:delay;delay=requested;ramp=rampLength;
    }
    void process(float* left,float* right,int count) noexcept {
        if(ring[0].empty())return;
        const auto size=ring[0].size();
        for(int i=0;i<count;++i){
            const float blend=ramp>0?1-static_cast<float>(ramp)/rampLength:1;
            for(int c=0;c<2;++c){float* data=c==0?left:right;ring[c][position]=data?data[i]:0;
                const auto tap=[&](int d){return ring[c][(position+size-static_cast<size_t>(d))%size];};
                if(data)data[i]=tap(oldDelay)*(1-blend)+tap(delay)*blend;
            }
            if(ramp>0)--ramp;position=(position+1)%size;
        }
    }
private:
    std::array<std::vector<float>,2> ring;double rate=48000;size_t position=0;int delay=0,oldDelay=0,ramp=0,rampLength=2400;bool initialised=false;
};
}
