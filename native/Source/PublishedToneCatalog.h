#pragma once

#include <array>
#include <string_view>

namespace fengyin
{
enum class PublishedToneKind { swamStyle, swamPackage, kongProject, kongPackage, kontaktPackage };

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
inline constexpr std::array<PublishedToneDefinition, 33> publishedToneCatalog {{
    { "soprano-kenny",       "高萨-原声",       "soprano-sax",       "kenny",          PublishedToneKind::swamPackage, "soprano-kenny.fytonepack" },
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
    { "violin",              "激情小提琴",       "violin",           "natural",        PublishedToneKind::swamPackage,   "violin-sweet.fytonepack" },
    { "violin-soft",         "柔美小提琴",       "violin",           "natural",        PublishedToneKind::swamPackage,   "violin-soft.fytonepack" },
    { "cello",               "大提琴",       "cello",            "natural",        PublishedToneKind::swamPackage,   "cello.fytonepack" },
    { "kong-erhu",           "二胡",          "kong-erhu-2",      "release",        PublishedToneKind::kongPackage, "folk-erhu.fytonepack" },
    { "kong-hulusi",         "葫芦丝",         "kong-hulusi",      "release",        PublishedToneKind::kongPackage, "folk-hulusi.fytonepack" },
    { "kong-liuqin",         "柳琴",          "kong-liuqin",      "release",        PublishedToneKind::kongPackage, "folk-liuqin.fytonepack" },
    { "kong-matouqin",       "马头琴",         "kong-matouqin",    "release",        PublishedToneKind::kongPackage, "folk-matouqin.fytonepack" },
    { "kong-qudi",           "曲笛",          "kong-dizi",        "release",        PublishedToneKind::kongPackage, "folk-qudi.fytonepack" },
    { "kong-xun",            "埙",           "kong-xun",         "release",        PublishedToneKind::kongPackage, "folk-xun.fytonepack" },
    { "kong-suona",          "唢呐",          "kong-suona-2",     "release",        PublishedToneKind::kongPackage, "folk-suona.fytonepack" },
    { "kong-sanxian",        "三弦",          "kong-sanxian",     "release",        PublishedToneKind::kongPackage, "folk-sanxian.fytonepack" },
    { "kong-pipa",           "琵琶",          "kong-pipa",        "release",        PublishedToneKind::kongPackage, "folk-pipa.fytonepack" },
    { "kong-nanxiao",        "南箫",          "kong-nanxiao",     "release",        PublishedToneKind::kongPackage, "folk-nanxiao.fytonepack" },
    { "kong-dongxiao",       "洞箫",          "kong-xiao",        "release",        PublishedToneKind::kongPackage, "folk-dongxiao.fytonepack" },
    { "kong-bangdi",         "梆笛",          "kong-dizi-2",      "release",        PublishedToneKind::kongPackage, "folk-bangdi.fytonepack" },
    { "kong-jinghu",         "京胡",          "kong-jinghu",      "release",        PublishedToneKind::kongPackage, "folk-jinghu.fytonepack" },
    { "kontakt-kensa", "肯萨", "kontakt-kensa", "release", PublishedToneKind::kontaktPackage, "kontakt-kensa.fytonepack" },
}};
}
