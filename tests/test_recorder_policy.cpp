#include "../native/Recorder/RecorderPolicy.h"
#include <stdexcept>
#include <cmath>
#include <utility>
using namespace fengyin::capture;
void require(bool ok){if(!ok)throw std::runtime_error("Recorder policy regression");}
int main(){
    require(backend("")==Backend::thirdParty);
    require(backend("legacy")==Backend::legacy);
    require(backend("unknown")==Backend::thirdParty);
    for(auto [w,h]:{std::pair{1920,1080},std::pair{3840,2160},std::pair{1366,768},std::pair{1080,1920}}){
        const auto size=canvas(w,h);
        require(size.width<=1920 && size.height<=1080);
        require(size.width<=w && size.height<=h);
        require(size.width%2==0 && size.height%2==0);
        require(std::abs(double(size.width)/size.height-double(w)/h)<0.005);
    }
    require(canvas(3840,2160).width==1920 && canvas(3840,2160).height==1080);
    require(fps==24 && audioRate==48000 && audioChannels==2 && audioBitrateBytes==24000);
}
