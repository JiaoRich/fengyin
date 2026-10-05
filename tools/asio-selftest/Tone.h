#pragma once
#include <cmath>
#include <cstdint>
#include <cstring>

namespace tone
{
// ASIO sample type values; no host framework or audio library dependency.
inline int bytes(int type)
{
    const int t = type >= 16 ? type - 16 : type;
    if (t == 0) return 2;
    if (t == 1) return 3;
    if (t == 2 || t == 3 || (t >= 8 && t <= 11)) return 4;
    return t == 4 ? 8 : 0;
}
inline void encode(unsigned char* dst, int type, double value)
{
    const auto t = type >= 16 ? type - 16 : type;
    const bool little = type >= 16;
    const int width = bytes(type);
    std::uint64_t bits = 0;
    if (t == 3) { float f = static_cast<float>(value); std::uint32_t b; std::memcpy(&b, &f, 4); bits = b; }
    else if (t == 4) std::memcpy(&bits, &value, 8);
    else
    {
        const int depth = t == 0 || t == 8 ? 16 : t == 1 || t == 11 ? 24 : t == 9 ? 18 : t == 10 ? 20 : 32;
        bits = static_cast<std::uint64_t>(static_cast<std::int64_t>(std::llround(value * (std::ldexp(1.0, depth - 1) - 1))));
    }
    for (int i = 0; i < width; ++i) dst[little ? i : width - i - 1] = static_cast<unsigned char>(bits >> (i * 8));
}
inline double sample(std::uint64_t frame)
{
    if (frame >= 96000) return 0;
    double envelope = 1;
    if (frame < 480) envelope = frame / 480.0;
    if (96000 - frame < 480) envelope = (96000 - frame) / 480.0;
    return 0.0316227766 * envelope * std::sin(6.283185307179586 * 440 * frame / 48000.0);
}
}
