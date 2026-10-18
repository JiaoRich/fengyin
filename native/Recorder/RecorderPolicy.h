#pragma once
#include <algorithm>
#include <string_view>

namespace fengyin::capture {
enum class Backend { thirdParty, legacy };
inline Backend backend(std::string_view value) noexcept {
    return value=="legacy" ? Backend::legacy : Backend::thirdParty;
}
struct Size {int width,height;};
inline Size canvas(int width,int height) noexcept {
    width=std::max(2,width);height=std::max(2,height);
    const double scale=std::min({1.0,1920.0/width,1080.0/height});
    return {std::max(2,int(width*scale)/2*2),std::max(2,int(height*scale)/2*2)};
}
inline constexpr int fps=24, bitrate=8000000, audioRate=48000, audioChannels=2;
inline constexpr int audioBitrateBytes=24000;
}
