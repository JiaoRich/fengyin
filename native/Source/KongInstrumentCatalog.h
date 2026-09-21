#pragma once

#include "MasterOutputService.h"
#include <juce_core/juce_core.h>

namespace fengyin
{
struct KongInstrumentDefinition
{
    const char* key;
    const char* chineseName;
    const char* mark;
    const char* programAliases;
    InstrumentMixProfile profile;
};

class KongInstrumentCatalog
{
public:
    static const std::array<KongInstrumentDefinition, 16>& all()
    {
        static const std::array<KongInstrumentDefinition, 16> items {{
            { "kong-dizi", "竹笛", "笛", "dizi|di zi|bamboo flute", InstrumentMixProfile::woodwind },
            { "kong-xiao", "洞箫", "箫", "xiao|dong xiao", InstrumentMixProfile::woodwind },
            { "kong-hulusi", "葫芦丝", "丝", "hulusi|hulu si", InstrumentMixProfile::woodwind },
            { "kong-bawu", "巴乌", "乌", "bawu|ba wu", InstrumentMixProfile::woodwind },
            { "kong-suona", "唢呐", "呐", "suona|suo na", InstrumentMixProfile::brass },
            { "kong-guanzi", "管子", "管", "guanzi|guan zi", InstrumentMixProfile::woodwind },
            { "kong-sheng", "笙", "笙", "sheng", InstrumentMixProfile::woodwind },
            { "kong-xun", "埙", "埙", "xun", InstrumentMixProfile::woodwind },
            { "kong-erhu", "二胡", "胡", "erhu|er hu", InstrumentMixProfile::strings },
            { "kong-gaohu", "高胡", "高", "gaohu|gao hu", InstrumentMixProfile::strings },
            { "kong-jinghu", "京胡", "京", "jinghu|jing hu", InstrumentMixProfile::strings },
            { "kong-matouqin", "马头琴", "马", "matouqin|morin khuur|horse head", InstrumentMixProfile::strings },
            { "kong-pipa", "琵琶", "琵", "pipa|pi pa", InstrumentMixProfile::strings },
            { "kong-guzheng", "古筝", "筝", "guzheng|gu zheng|zheng", InstrumentMixProfile::strings },
            { "kong-guqin", "古琴", "琴", "guqin|gu qin", InstrumentMixProfile::strings },
            { "kong-ruan", "中阮", "阮", "ruan|zhongruan|zhong ruan", InstrumentMixProfile::strings }
        }};
        return items;
    }

    static const KongInstrumentDefinition* find(const juce::String& key)
    {
        for (const auto& item : all())
            if (key == item.key) return &item;
        return nullptr;
    }
};

class SupportedInstrumentClassifier
{
public:
    enum class Brand { unsupported, swam, kong };

    static Brand classify(const juce::String& name, const juce::String& manufacturer = {})
    {
        const auto text = (name + " " + manufacturer).toLowerCase().removeCharacters(" ._-()");
        if (text.contains("swam") || text.contains("audiomodeling")) return Brand::swam;
        if (text.contains("kongaudio") || text.contains("qinengine") || text.contains("qin3")) return Brand::kong;
        return Brand::unsupported;
    }

    static bool isSupported(const juce::String& name, const juce::String& manufacturer = {})
    {
        return classify(name, manufacturer) != Brand::unsupported;
    }
};
}
