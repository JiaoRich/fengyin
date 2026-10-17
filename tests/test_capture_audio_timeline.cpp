#include "../native/ScreenRecorder/AudioTimeline.h"
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
    std::cout<<"Audio timeline: 60000 jittered packets preserve continuity; device gaps retained\n";
}
