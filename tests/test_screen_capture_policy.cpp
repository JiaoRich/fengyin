#include "../native/ScreenCapture/CapturePolicy.h"
#include <cmath>
#include <utility>
using namespace fengyin::screen;
void require(bool ok){if(!ok)throw std::runtime_error("Screen capture policy regression");}
int main(){
    for(auto [w,h]:{std::pair{1920,1080},std::pair{3840,2160},std::pair{1366,768},std::pair{1080,1920}}){
        const auto s=canvas(w,h);
        require(s.width<=1920 && s.height<=1080 && s.width<=w && s.height<=h);
        require(s.width%2==0 && s.height%2==0);
        require(std::abs(double(s.width)/s.height-double(w)/h)<.005);
    }
    bool rejected=false;try{canvas(0,1080);}catch(const std::invalid_argument&){rejected=true;}require(rejected);
    FrameHealth health;
    for(int n=0;n<900;++n)health.observe(n*second/fps);
    require(health.frames==900 && health.late==0);
    rejected=false;try{health.observe(health.previous);}catch(const std::runtime_error&){rejected=true;}require(rejected);
    health.observe(health.previous+second);require(health.degraded());
    FrameHealth jitter;std::int64_t t=0;
    for(int n=0;n<600;++n){jitter.observe(t);t+=(n%20==0?second/10:second/fps);}
    require(jitter.degraded() && jitter.frames==600); // Preserve a usable recording.
    const std::int64_t origin=123456789;
    for(int n=0;n<900;++n){
        const auto now=origin+n*second/fps+10000; // 1 ms jitter must not accumulate.
        const auto deadline=nextFrameDeadline(origin,now);
        require(deadline==origin+(n+1)*second/fps && deadline>now);
    }
    require(nextFrameDeadline(origin,origin+second+1234)==origin+31*second/fps);
}
