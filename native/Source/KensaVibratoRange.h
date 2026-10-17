#pragma once
#include <algorithm>
#include <cmath>
namespace fengyin {
// Verified against Kenny G PRO's script and the user's held-note test.
// Do not apply this library-specific range to arbitrary Kontakt libraries.
inline int kensaVibratoController(float depth) noexcept {
    if (!std::isfinite(depth) || depth<=0.f) return 0;
    depth=std::clamp(depth,0.f,1.f);
    // 70 is the user's confirmed periodic-vibrato endpoint. Driving all the
    // way to 96 produced excessive pitch motion in the shipped profile.
    return std::clamp(30+static_cast<int>(std::lround(depth*40.f)),31,70);
}
// Supplied KSP: 150 phase entries, 4 ms loop, phase increment
// 1400 + CC19 * 2400 / 127. This is nominal steady-state frequency;
// the library intentionally adds humanisation and an internal start ramp.
inline double kensaNominalVibratoHz(int cc) noexcept {
    return (1400.0+std::clamp(cc,0,127)*2400.0/127.0)/600.0;
}
inline int kensaVibratoRateController(float hz) noexcept {
    if(!std::isfinite(hz))return 0;
    return std::clamp(static_cast<int>(std::lround((hz*600.0-1400.0)*127.0/2400.0)),0,127);
}
}
