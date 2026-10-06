#pragma once

#include <array>
#include <string_view>

namespace fengyin
{
enum class PublishedToneKind { swamStyle, swamPackage, kongProject };

struct PublishedToneDefinition
{
    std::string_view id;
    std::string_view name;
    std::string_view instrumentKey;
    std::string_view styleId;
    PublishedToneKind kind;
    std::string_view assetName;
};

// The release catalogue is deliberately explicit. Installing another plug-in
// must never make an unreviewed tone appear in the customer-facing library.
inline constexpr std::array<PublishedToneDefinition, 37> publishedToneCatalog {{
    { "soprano-kenny",       "高萨-肯萨",       "soprano-sax",       "kenny",          PublishedToneKind::swamStyle,   {} },
    { "soprano-pop",         "高萨-流行",       "soprano-sax",       "pop-high",       PublishedToneKind::swamStyle,   {} },
    { "alto-jazz",           "中萨-爵士",       "alto-sax",          "jazz",           PublishedToneKind::swamStyle,   {} },
    { "alto-deep",           "中萨-深情",       "alto-sax",          "deep",           PublishedToneKind::swamStyle,   {} },
    { "alto-pop",            "中萨-流行",       "alto-sax",          "pop",            PublishedToneKind::swamStyle,   {} },
    { "tenor-mellow",        "次中萨-醇厚",     "tenor-sax",         "mellow",         PublishedToneKind::swamStyle,   {} },
    { "tenor-warm",          "次中萨-温暖",     "tenor-sax",         "warm",           PublishedToneKind::swamStyle,   {} },
    { "tenor-air-pocket",    "次中萨-气包音",   "tenor-sax",         "air-pocket",     PublishedToneKind::swamPackage, "tenor-air-pocket.fytonepack" },
    { "baritone-jazz",       "上低萨-爵士",     "baritone-sax",      "jazz",           PublishedToneKind::swamStyle,   {} },
    { "baritone-pop",        "上低萨-流行",     "baritone-sax",      "pop",            PublishedToneKind::swamStyle,   {} },
    { "trumpet",             "小号",          "trumpet",          "natural",        PublishedToneKind::swamStyle,   {} },
    { "piccolo-trumpet",     "高音小号",        "piccolo-trumpet",  "natural",        PublishedToneKind::swamStyle,   {} },
    { "alto-trombone",       "中音长号",        "alto-trombone",    "natural",        PublishedToneKind::swamStyle,   {} },
    { "bass-trombone",       "低音长号",        "bass-trombone",    "natural",        PublishedToneKind::swamStyle,   {} },
    { "flute",               "长笛",          "flute",            "natural",        PublishedToneKind::swamStyle,   {} },
    { "piccolo",             "短笛",          "piccolo",          "natural",        PublishedToneKind::swamStyle,   {} },
    { "clarinet",            "单簧管",         "clarinet",         "natural",        PublishedToneKind::swamStyle,   {} },
    { "oboe",                "双簧管",         "oboe",             "natural",        PublishedToneKind::swamStyle,   {} },
    { "bassoon",             "巴松管",         "bassoon",          "natural",        PublishedToneKind::swamStyle,   {} },
    { "violin",              "小提琴独奏",       "violin",           "natural",        PublishedToneKind::swamStyle,   {} },
    { "viola",               "中提琴独奏",       "viola",            "natural",        PublishedToneKind::swamStyle,   {} },
    { "cello",               "大提琴独奏",       "cello",            "natural",        PublishedToneKind::swamStyle,   {} },
    { "violin-section",      "小提琴重奏",       "violin-section",   "natural",        PublishedToneKind::swamStyle,   {} },
    { "viola-section",       "中提琴重奏",       "viola-section",    "natural",        PublishedToneKind::swamStyle,   {} },
    { "cello-section",       "大提琴重奏",       "cello-section",    "natural",        PublishedToneKind::swamStyle,   {} },
    { "kong-erhu",           "二胡",          "kong-erhu",        "release",        PublishedToneKind::kongProject, "erhu.kam" },
    { "kong-guzheng",        "古筝",          "kong-guzheng",     "release",        PublishedToneKind::kongProject, "guzheng.kam" },
    { "kong-hulusi",         "葫芦丝",         "kong-hulusi",      "release",        PublishedToneKind::kongProject, "hulusi.kam" },
    { "kong-liuqin",         "柳琴",          "kong-liuqin",      "release",        PublishedToneKind::kongProject, "liuqin.kam" },
    { "kong-matouqin",       "马头琴",         "kong-matouqin",    "release",        PublishedToneKind::kongProject, "matouqin.kam" },
    { "kong-qudi",           "曲笛",          "kong-dizi",        "release",        PublishedToneKind::kongProject, "qudi.kam" },
    { "kong-xun",            "埙",           "kong-xun",         "release",        PublishedToneKind::kongProject, "xun.kam" },
    { "kong-suona",          "唢呐",          "kong-suona",       "release",        PublishedToneKind::kongProject, "suona.kam" },
    { "kong-sanxian",        "三弦",          "kong-sanxian",     "release",        PublishedToneKind::kongProject, "sanxian.kam" },
    { "kong-pipa",           "琵琶",          "kong-pipa",        "release",        PublishedToneKind::kongProject, "pipa.kam" },
    { "kong-sheng",          "笙",           "kong-sheng",       "release",        PublishedToneKind::kongProject, "sheng.kam" },
    { "kong-nanxiao",        "南箫",          "kong-nanxiao",     "release",        PublishedToneKind::kongProject, "nanxiao.kam" },
}};
}
