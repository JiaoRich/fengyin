#pragma once
#include <cstdint>

namespace fengyin {
// A viewport measured before a resize/minimise/reload must never reappear.
// Commands within a layout epoch are ordered, including immediate page hides.
class VideoViewportGate {
public:
    void invalidate() noexcept { ++epoch; revision=-1; visible=false; }
    bool apply(std::int64_t measuredEpoch,std::int64_t serial,bool requested) noexcept {
        if(measuredEpoch!=epoch || serial<0 || serial<=revision)return false;
        revision=serial;visible=requested;return true;
    }
    std::int64_t currentEpoch() const noexcept {return epoch;}
    bool isVisible() const noexcept {return visible;}
private:
    std::int64_t epoch=1,revision=-1;
    bool visible=false;
};
}
