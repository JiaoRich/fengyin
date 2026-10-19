#include "../native/Recorder/RecorderPolicy.h"
#include "../native/Recorder/CapturePacketCopy.h"
#include "../native/Source/VideoViewportGate.h"
#include "../native/Recorder/RecordingStageTiming.h"
#include <array>
#include <stdexcept>
#include <cmath>
#include <utility>
using namespace fengyin::capture;
void require(bool ok){if(!ok)throw std::runtime_error("Recorder policy regression");}
int main(){
    RecordingStageTiming timing;
    timing.audio(100,8);timing.frame(105);
    timing.audio(140,0);timing.frame(160);
    const auto measured=timing.take(180);
    require(measured.audioPackets==2 && measured.paddedBytes==8);
    require(measured.postAudioToFrameMaxMs==20 && measured.frameIntervalMaxMs==55);
    require(measured.audioGapMs==40 && measured.frameGapMs==20);
    const auto reset=timing.take(200);
    require(reset.audioPackets==0 && reset.postAudioToFrameMaxMs==0 && reset.frameGapMs==40);
    fengyin::VideoViewportGate gate;
    const auto first=gate.currentEpoch();
    require(gate.apply(first,1,true) && gate.isVisible());
    require(gate.apply(first,3,false) && !gate.isVisible());
    require(!gate.apply(first,2,true) && !gate.isVisible());
    gate.invalidate();
    require(!gate.apply(first,4,true) && !gate.isVisible());
    require(gate.apply(gate.currentEpoch(),4,true) && gate.isVisible());
    gate.invalidate();
    require(!gate.isVisible());
    require(!gate.apply(gate.currentEpoch(),-1,true));
    std::array<unsigned char,24> packet{}, output{};
    packet.fill(0x7f);output.fill(0xcc);
    require(copyCapturePacket(output.data(),output.size(),nullptr,16,true));
    for(int i=0;i<16;++i)require(output[i]==0);
    for(int i=16;i<24;++i)require(output[i]==0xcc);
    require(copyCapturePacket(output.data(),output.size(),packet.data(),24,true));
    for(auto v:output)require(v==0);
    for(auto v:packet)require(v==0x7f);
    require(copyCapturePacket(output.data(),output.size(),packet.data(),24,false));
    require(output==packet);
    require(!copyCapturePacket(output.data(),output.size(),nullptr,24,false));
    require(!copyCapturePacket(output.data(),8,packet.data(),24,false));
    require(copyCapturePacket(nullptr,0,nullptr,0,true));
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
