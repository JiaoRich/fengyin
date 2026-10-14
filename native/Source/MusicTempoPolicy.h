#pragma once
#include <algorithm>
#include <cmath>
namespace fengyin {
// Three simple presets. Boundaries: slow <80, medium 80..120, fast >120 BPM.
struct MusicTempoPolicy {
    int bpm; double waitMs, fadeMs; float saxRate, stringsRate, predelayMs;
    static int valid(int mode) noexcept { return mode>=0&&mode<=2?mode:1; }
    static MusicTempoPolicy forMode(int mode) noexcept {
        switch(valid(mode)) {
            case 0:return {70,60000.0/70,15000.0/70,4.5f,5.0f,60.0f};
            case 2:return {140,45000.0/140,15000.0/140,5.5f,6.0f,25.0f};
            default:return {100,450,150,5.0f,5.5f,40.0f};
        }
    }
    float fade(double elapsedMs) const noexcept {
        if(!std::isfinite(elapsedMs))return 0;
        const auto t=std::clamp((elapsedMs-waitMs)/fadeMs,0.0,1.0);
        return static_cast<float>(t*t*(3-2*t));
    }
};
}
