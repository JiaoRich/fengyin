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
    static const std::array<KongInstrumentDefinition, 27>& all()
    {
        static const std::array<KongInstrumentDefinition, 27> items {{
            { "kong-dizi", "竹笛一", "笛", "dizi_1|dizi 1|dizi1|di zi 1|bamboo flute 1|dizi", InstrumentMixProfile::woodwind },
            { "kong-dizi-2", "竹笛二", "笛", "dizi_2|dizi 2|dizi2|di zi 2|bamboo flute 2", InstrumentMixProfile::woodwind },
            { "kong-xiao", "洞箫", "箫", "xiao|dong xiao", InstrumentMixProfile::woodwind },
            { "kong-nanxiao", "南箫", "南", "nanxiao|nan xiao", InstrumentMixProfile::woodwind },
            { "kong-xun", "埙", "埙", "xun", InstrumentMixProfile::woodwind },
            { "kong-guanzi", "管子", "管", "guanzi|guan zi", InstrumentMixProfile::woodwind },
            { "kong-hulusi", "葫芦丝一", "丝", "hulusi_1|hulusi 1|hulusi1|hulu si 1|hulusi", InstrumentMixProfile::woodwind },
            { "kong-hulusi-2", "葫芦丝二", "丝", "hulusi_2|hulusi 2|hulusi2|hulu si 2", InstrumentMixProfile::woodwind },
            { "kong-bawu", "巴乌", "乌", "bawu|ba wu", InstrumentMixProfile::woodwind },
            { "kong-sheng", "笙", "笙", "sheng", InstrumentMixProfile::woodwind },
            { "kong-suona", "唢呐一", "呐", "suona_1|suona 1|suona1|suo na 1|suona", InstrumentMixProfile::brass },
            { "kong-suona-2", "唢呐二", "呐", "suona_2|suona 2|suona2|suo na 2", InstrumentMixProfile::brass },
            { "kong-guqin", "古琴", "琴", "guqin|gu qin", InstrumentMixProfile::strings },
            { "kong-guzheng", "古筝", "筝", "guzheng|gu zheng|zheng", InstrumentMixProfile::strings },
            { "kong-liuqin", "柳琴", "柳", "liuqin|liu qin", InstrumentMixProfile::strings },
            { "kong-pipa", "琵琶", "琵", "pipa|pi pa", InstrumentMixProfile::strings },
            { "kong-ruan", "中阮一", "阮", "ruan_1|ruan 1|ruan1|zhongruan 1|ruan", InstrumentMixProfile::strings },
            { "kong-ruan-2", "中阮二", "阮", "ruan_2|ruan 2|ruan2|zhongruan 2", InstrumentMixProfile::strings },
            { "kong-sanxian", "三弦", "弦", "sanxian|san xian", InstrumentMixProfile::strings },
            { "kong-erhu", "二胡一", "胡", "erhu_1|erhu 1|erhu1|er hu 1|erhu", InstrumentMixProfile::strings },
            { "kong-erhu-2", "二胡二", "胡", "erhu_2|erhu 2|erhu2|er hu 2", InstrumentMixProfile::strings },
            { "kong-gaohu", "高胡", "高", "gaohu|gao hu", InstrumentMixProfile::strings },
            { "kong-jinghu", "京胡", "京", "jinghu|jing hu", InstrumentMixProfile::strings },
            { "kong-zhonghu", "中胡", "中", "zhonghu|zhong hu", InstrumentMixProfile::strings },
            { "kong-matouqin", "马头琴", "马", "matouqin|morin khuur|horse head", InstrumentMixProfile::strings },
            { "kong-yangqin", "扬琴", "扬", "yangqin|yang qin", InstrumentMixProfile::strings },
            { "kong-misc", "综合民乐", "乐", "kong_misc|kong misc|misc", InstrumentMixProfile::generic }
        }};
        return items;
    }

    static const KongInstrumentDefinition* find(const juce::String& key)
    {
        for (const auto& item : all())
            if (key == item.key) return &item;
        return nullptr;
    }

    static const KongInstrumentDefinition* matchProgram(const juce::String& programName)
    {
        const auto normalised = programName.toLowerCase().removeCharacters(" ._-()");
        if (normalised.isEmpty()) return nullptr;
        const KongInstrumentDefinition* best = nullptr;
        int bestLength = 0;
        for (const auto& item : all())
        {
            if (normalised == juce::String::fromUTF8(item.chineseName)) return &item;
            const auto aliases = juce::StringArray::fromTokens(item.programAliases, "|", "");
            for (const auto& alias : aliases)
            {
                const auto candidate = alias.toLowerCase().removeCharacters(" ._-()");
                if (candidate.length() > bestLength && normalised.contains(candidate))
                {
                    best = &item;
                    bestLength = candidate.length();
                }
            }
        }
        return best;
    }
};

class SupportedInstrumentClassifier
{
public:
    enum class Brand { unsupported, swam, kong };

    static Brand classify(const juce::String& name, const juce::String& manufacturer = {},
                          const juce::String& fileOrIdentifier = {})
    {
        const auto text = (name + " " + manufacturer).toLowerCase().removeCharacters(" ._-()");
        if (text.contains("swam") || text.contains("audiomodeling")) return Brand::swam;
        if (text.contains("kongaudio") || text.contains("qinengine") || text.contains("qin3")
            || text.contains(juce::String::fromUTF8("空音"))) return Brand::kong;
        const auto filename = fileOrIdentifier.replaceCharacter('\\', '/')
            .fromLastOccurrenceOf("/", false, false);
        if (filename.equalsIgnoreCase("QinEngineV3.vst3")) return Brand::kong;
        return Brand::unsupported;
    }

    static bool isSupported(const juce::String& name, const juce::String& manufacturer = {},
                            const juce::String& fileOrIdentifier = {})
    {
        return classify(name, manufacturer, fileOrIdentifier) != Brand::unsupported;
    }
};
}
