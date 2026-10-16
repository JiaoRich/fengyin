#pragma once
#include <algorithm>
#include <cmath>
namespace fengyin {
// Verified against Kenny G PRO's script and the user's held-note test.
// Do not apply this library-specific range to arbitrary Kontakt libraries.
inline int kensaVibratoController(float depth) noexcept {
    if (!std::isfinite(depth) || depth<=0.f) return 0;
    depth=std::clamp(depth,0.f,1.f);
    return std::clamp(30+static_cast<int>(std::lround(depth*66.f)),31,96);
}
}
