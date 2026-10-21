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
// Absolute pacing avoids accumulating scheduler jitter. Skip expired slots,
// rather than burst-encoding old frames and competing with live audio.
inline std::int64_t nextFrameDeadline(std::int64_t origin,std::int64_t now) {
    auto slot=(now-origin)*fps/second+1;
    if(origin+slot*second/fps<=now)++slot;
    return origin+slot*second/fps;
}
// Submission timestamps measure delivered pacing, not Windows capture latency.
struct FrameHealth {
    std::int64_t previous=-1;
    std::uint64_t frames=0,late=0,maxGap=0;
    void observe(std::int64_t time) {
        if(time<0 || (previous>=0 && time<=previous))throw std::runtime_error("Non-monotonic video timestamp");
        if(previous>=0) {
            const auto gap=static_cast<std::uint64_t>(time-previous);
            maxGap=std::max(maxGap,gap);
            if(gap>second/fps*3/2)++late;
        }
        previous=time;++frames;
    }
    bool degraded() const {return maxGap>second/5 || (frames>=300 && late*100>frames);}
};
}
