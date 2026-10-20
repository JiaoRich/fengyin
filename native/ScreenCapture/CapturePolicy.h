#pragma once
#include <algorithm>
#include <cstdint>
#include <stdexcept>

namespace fengyin::screen {
constexpr int fps=30, bitrate=10000000, audioRate=48000, audioChannels=2;
constexpr std::int64_t second=10000000;
struct Size { int width,height; };
inline Size canvas(int w,int h) {
    if(w<2||h<2)throw std::invalid_argument("Invalid display dimensions");
    const double scale=std::min({1.0,1920.0/w,1080.0/h});
    return {std::max(2,int(w*scale)/2*2),std::max(2,int(h*scale)/2*2)};
}
// Never infer smoothness from the nominal MP4 frame rate. Count missed slots
// against capture timestamps, not encoder completion timestamps.
struct FrameHealth {
    std::int64_t previous=-1;
    std::uint64_t frames=0,late=0,maxGap=0;
    void observe(std::int64_t time) {
        if(time<0 || (previous>=0 && time<=previous))throw std::runtime_error("Non-monotonic video timestamp");
        if(previous>=0) {
            const auto gap=static_cast<std::uint64_t>(time-previous);
            maxGap=std::max(maxGap,gap);
            if(gap>second/fps*3/2)++late;
            if(gap>second/5)throw std::runtime_error("Video stalled for over 200ms; partial recording retained");
        }
        previous=time;++frames;
    }
};
}
