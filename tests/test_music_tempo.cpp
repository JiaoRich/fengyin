#include "MusicTempoPolicy.h"
#include "WetPreDelay.h"
#include <cassert>
#include <limits>
#include <vector>
int main(){
    using namespace fengyin;
    for(int mode=0;mode<3;++mode){const auto p=MusicTempoPolicy::forMode(mode);
        assert(p.fade(0)==0&&p.fade(p.waitMs)==0&&p.fade(p.waitMs+p.fadeMs)==1);
        assert(std::abs(p.fade(p.waitMs+p.fadeMs/2)-.5)<1e-6);float previous=0;
        for(int t=0;t<3000;++t){float f=p.fade(t);assert(f>=previous&&f<=1);previous=f;}
        for(double rate:{44100.,48000.,96000.}){
            WetPreDelay d;d.prepare(rate);d.setDelay(p.predelayMs);int n=(int)(rate*.1);std::vector<float> l(n),r(n);l[0]=1;r[0]=.5;
            d.process(l.data(),r.data(),n);int delay=(int)std::round(rate*p.predelayMs/1000);
            for(int i=0;i<n;++i){assert(std::abs(l[i]-(i==delay?1.f:0.f))<1e-6);assert(std::abs(r[i]-(i==delay?.5f:0.f))<1e-6);}
            d.setDelay(25);std::fill(l.begin(),l.end(),.1f);std::fill(r.begin(),r.end(),.1f);d.process(l.data(),r.data(),n);for(float x:l)assert(std::isfinite(x)&&std::abs(x)<=.1f+1e-6);
            d.reset();std::fill(l.begin(),l.end(),0);d.process(l.data(),nullptr,n);for(float x:l)assert(x==0);
        }
    }
    assert(MusicTempoPolicy::valid(-1)==1&&MusicTempoPolicy::valid(9)==1);
    assert(MusicTempoPolicy::forMode(1).fade(std::numeric_limits<double>::quiet_NaN())==0);
}
