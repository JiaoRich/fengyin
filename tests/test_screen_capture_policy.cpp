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
    rejected=false;try{health.observe(health.previous+second);}catch(const std::runtime_error&){rejected=true;}require(rejected);
}
