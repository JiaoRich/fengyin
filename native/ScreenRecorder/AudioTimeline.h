#pragma once
#include <algorithm>
#include <cstdint>

// Device positions are monotonic sample frames. Anchor to QPC once, rather
// than inserting/trimming samples for small per-packet QPC fluctuations.
struct CaptureAudioTimeline {
    bool anchored=false;
    uint64_t firstPosition=0;
    int64_t firstFrame=0,nextFrame=0;
    int64_t packet(uint64_t position,int64_t qpc,int64_t origin,bool validTime,uint32_t frames) {
        if(!anchored) {
            anchored=true;firstPosition=position;
            firstFrame=validTime?(qpc-origin)*48000/10000000:nextFrame;
        }
        const auto start=position>=firstPosition
            ?firstFrame+static_cast<int64_t>(position-firstPosition):nextFrame;
        nextFrame=start+frames;
        return start;
    }
};
