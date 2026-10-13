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
inline constexpr std::array<PublishedToneDefinition, 30> publishedToneCatalog {{
    { "soprano-kenny",       "高萨-肯尼基",     "soprano-sax",       "kenny",          PublishedToneKind::swamPackage, "soprano-kenny.fytonepack" },
    { "soprano-pop",         "高萨-流行",       "soprano-sax",       "pop-high",       PublishedToneKind::swamStyle,   {} },
    { "alto-jazz",           "中萨-爵士",       "alto-sax",          "jazz",           PublishedToneKind::swamPackage,   "alto-jazz.fytonepack" },
    { "alto-deep",           "中萨-深情",       "alto-sax",          "deep",           PublishedToneKind::swamPackage,   "alto-deep.fytonepack" },
    { "alto-pop",            "中萨-流行",       "alto-sax",          "pop",            PublishedToneKind::swamPackage,   "alto-pop.fytonepack" },
    { "tenor-mellow",        "次中萨-醇厚",     "tenor-sax",         "mellow",         PublishedToneKind::swamPackage,   "tenor-mellow.fytonepack" },
    { "tenor-warm",          "次中萨-温暖",     "tenor-sax",         "warm",           PublishedToneKind::swamPackage,   "tenor-warm.fytonepack" },
    { "tenor-air-pocket",    "次中萨-气包音",   "tenor-sax",         "air-pocket",     PublishedToneKind::swamPackage, "tenor-air-pocket.fytonepack" },
    { "trumpet",             "小号",          "trumpet",          "natural",        PublishedToneKind::swamPackage,   "trumpet.fytonepack" },
    { "piccolo-trumpet",     "高音小号",        "piccolo-trumpet",  "natural",        PublishedToneKind::swamPackage,   "piccolo-trumpet.fytonepack" },
    { "alto-trombone",       "中音长号",        "alto-trombone",    "natural",        PublishedToneKind::swamPackage,   "alto-trombone.fytonepack" },
    { "bass-trombone",       "低音长号",        "bass-trombone",    "natural",        PublishedToneKind::swamPackage,   "bass-trombone.fytonepack" },
    { "flute",               "长笛",          "flute",            "natural",        PublishedToneKind::swamPackage,   "flute.fytonepack" },
    { "piccolo",             "短笛",          "piccolo",          "natural",        PublishedToneKind::swamPackage,   "piccolo.fytonepack" },
    { "clarinet",            "单簧管",         "clarinet",         "natural",        PublishedToneKind::swamPackage,   "clarinet.fytonepack" },
    { "oboe",                "双簧管",         "oboe",             "natural",        PublishedToneKind::swamPackage,   "oboe.fytonepack" },
    { "violin",              "小提琴",       "violin",           "natural",        PublishedToneKind::swamPackage,   "violin.fytonepack" },
    { "cello",               "大提琴",       "cello",            "natural",        PublishedToneKind::swamPackage,   "cello.fytonepack" },
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
