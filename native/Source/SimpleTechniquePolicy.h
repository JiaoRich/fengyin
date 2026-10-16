#pragma once
#include "MidiPerformanceSink.h"
#include <string_view>
#include <algorithm>
namespace fengyin {
inline bool simpleVibrato(std::string_view key) {
    return key=="kontakt-kensa" || key.find("sax") != std::string_view::npos || key.find("violin") != std::string_view::npos
        || key.find("viola") != std::string_view::npos || key.find("cello") != std::string_view::npos
        || key.find("double-bass") != std::string_view::npos
        || key.find("flute") != std::string_view::npos || key=="piccolo"
        || key.find("clarinet") != std::string_view::npos || key=="oboe" || key=="english-horn"
        || key.find("bassoon") != std::string_view::npos || key.find("trumpet") != std::string_view::npos
        || key.find("trombone") != std::string_view::npos || key.find("flugelhorn") != std::string_view::npos
        || key.find("tuba") != std::string_view::npos || key=="euphonium" || key=="horn-f" || key=="horn-bb"
        || key=="kong-erhu" || key=="kong-erhu-2" || key=="kong-matouqin"
        || key=="kong-hulusi" || key=="kong-hulusi-2" || key=="kong-xun"
        || key=="kong-nanxiao" || key=="kong-xiao" || key=="kong-dizi"
        || key=="kong-dizi-2" || key=="kong-suona" || key=="kong-suona-2" || key=="kong-jinghu";
}
inline bool simpleGrowl(std::string_view key) {
    return key == "alto-sax" || key == "tenor-sax" || key == "baritone-sax";
}
inline PerformanceTechnique simpleOther(std::string_view key) {
    if(key=="violin" || key=="cello")return PerformanceTechnique::tremolo;
    if (key == "kong-erhu-2") key = "kong-erhu";
    if (key == "kong-guzheng-2") key = "kong-guzheng";
    if (key == "kong-dizi-2") key = "kong-dizi";
    if (key == "kong-hulusi-2") key = "kong-hulusi";
    if (key == "kong-suona-2") key = "kong-suona";
    if (key == "flute" || key == "piccolo" || key == "kong-dizi" || key == "kong-suona" || key == "kong-sheng") return PerformanceTechnique::flutter;
    if (key == "kong-guzheng" || key == "kong-liuqin" || key == "kong-pipa" || key == "kong-sanxian"
        || key=="kong-erhu" || key=="kong-jinghu") return PerformanceTechnique::tremolo;
    return PerformanceTechnique::count;
}
inline bool simpleRoleSupported(std::string_view key, PerformanceTechnique role) {
    if (role == PerformanceTechnique::vibrato) return simpleVibrato(key);
    if (role == PerformanceTechnique::growl) return simpleGrowl(key);
    return role == PerformanceTechnique::mute && simpleOther(key) != PerformanceTechnique::count;
}
inline float automaticVibratoFade(double elapsedMs) noexcept {
    const auto t = std::clamp((elapsedMs - 600.0) / 400.0, 0.0, 1.0);
    return static_cast<float>(t*t*(3.0-2.0*t));
}
}
