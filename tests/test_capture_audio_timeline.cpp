#include "../native/ScreenRecorder/AudioTimeline.h"
#include "../native/ScreenRecorder/AudioPacketAssembler.h"
#include "../native/ScreenRecorder/PcmWaveWriter.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <iostream>
int main() {
    CaptureAudioTimeline timeline;
    const int64_t origin=100000000;
    const uint64_t first=480000;
    // Ten minutes of 10 ms packets; +/-2 ms timestamp jitter must neither
    // delete real samples nor inject silence. Encoder scheduling is irrelevant.
    for(int i=0;i<60000;++i) {
        const auto qpc=origin+100000+i*100000LL+(i?((i%3)-1)*20000:0);
        assert(timeline.packet(first+i*480ULL,qpc,origin,true,480)==480+i*480LL);
    }
    const auto next=timeline.nextFrame;
    assert(timeline.packet(first+60000*480ULL+960,origin,origin,false,480)==next+960);
    CaptureAudioTimeline fallback;
    assert(fallback.packet(0,0,origin,false,480)==0);
    assert(fallback.packet(480,origin+123456,origin,true,480)==480);
    // Delayed delivery must not replace valid samples with wall-clock silence.
    CaptureAudioAssembler assembler;
    std::vector<int16_t> output;
    const auto write=[&](const auto& pcm,int64_t firstFrame){assert(firstFrame*2==static_cast<int64_t>(output.size()));output.insert(output.end(),pcm.begin(),pcm.end());};
    for(int i=0;i<600;++i)assembler.packet(i*480,std::vector<int16_t>(960,1234),write);
    assert(assembler.position()==288000);
    assert(std::all_of(output.begin(),output.end(),[](auto x){return x==1234;}));
    assembler.packet(288480,std::vector<int16_t>(960,2345),write);
    assert(output[576000]==0 && output[576960]==2345);
    const auto wave=std::filesystem::temp_directory_path()/"fengyin-system-audio-unit.wav";
    {PcmWaveWriter writer(wave);writer.append(output);writer.close();}
    assert(std::filesystem::file_size(wave)==44+output.size()*2);
    std::ifstream input(wave,std::ios::binary);char header[44]{};input.read(header,44);
    assert(std::string(header,4)=="RIFF" && std::string(header+8,4)=="WAVE");
    assert(static_cast<unsigned char>(header[34])==16 && header[22]==2);
    input.close();std::filesystem::remove(wave);
    std::cout<<"Audio timeline: 60000 jittered packets preserve continuity; device gaps retained\n";
}
