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
inline constexpr std::array<PublishedToneDefinition, 42> publishedToneCatalog {{
    { "soprano-kenny", "高萨-原声", "soprano-sax", "kenny", PublishedToneKind::swamPackage, "soprano-kenny.fytonepack" },
    { "soprano-pop", "高萨-明亮", "soprano-sax", "pop-high", PublishedToneKind::swamPackage, "soprano-pop.fytonepack" },
    { "alto-jazz", "中萨-爵士", "alto-sax", "jazz", PublishedToneKind::swamPackage, "alto-jazz.fytonepack" },
    { "alto-deep", "中萨-深情", "alto-sax", "deep", PublishedToneKind::swamPackage, "alto-deep.fytonepack" },
    { "alto-pop", "中萨-流行", "alto-sax", "pop", PublishedToneKind::swamPackage, "alto-pop.fytonepack" },
    { "alto-silky", "中萨-丝滑", "alto-sax", "release", PublishedToneKind::swamPackage, "tone137-alto-silky.fytonepack" },
    { "tenor-mellow", "次中萨-醇厚", "tenor-sax", "mellow", PublishedToneKind::swamPackage, "tenor-mellow.fytonepack" },
    { "tenor-warm", "次中萨-温暖", "tenor-sax", "warm", PublishedToneKind::swamPackage, "tenor-warm.fytonepack" },
    { "tenor-air-pocket", "次中萨-气包音", "tenor-sax", "air-pocket", PublishedToneKind::swamPackage, "tenor-air-pocket.fytonepack" },
    { "tenor-silky", "次中萨-丝滑", "tenor-sax", "release", PublishedToneKind::swamPackage, "tone137-tenor-silky.fytonepack" },
    { "trumpet", "小号1", "trumpet", "natural", PublishedToneKind::swamPackage, "trumpet.fytonepack" },
    { "trumpet-2", "小号2", "trumpet", "release", PublishedToneKind::swamPackage, "tone137-trumpet-2.fytonepack" },
    { "piccolo-trumpet", "高音小号", "piccolo-trumpet", "natural", PublishedToneKind::swamPackage, "piccolo-trumpet.fytonepack" },
    { "alto-trombone", "中音长号", "alto-trombone", "natural", PublishedToneKind::swamPackage, "alto-trombone.fytonepack" },
    { "bass-trombone", "低音长号", "bass-trombone", "natural", PublishedToneKind::swamPackage, "bass-trombone.fytonepack" },
    { "flute", "长笛", "flute", "natural", PublishedToneKind::swamPackage, "flute.fytonepack" },
    { "piccolo", "短笛", "piccolo", "natural", PublishedToneKind::swamPackage, "piccolo.fytonepack" },
    { "clarinet", "单簧管1", "clarinet", "natural", PublishedToneKind::swamPackage, "clarinet.fytonepack" },
    { "kong-clarinet-2", "单簧管2", "container:kong-v3:khka_woodwinds", "release", PublishedToneKind::kongPackage, "tone137-kong-clarinet-2.fytonepack" },
    { "oboe", "双簧管1", "oboe", "natural", PublishedToneKind::swamPackage, "oboe.fytonepack" },
    { "kong-oboe-2", "双簧管2", "container:kong-v3:khka_woodwinds", "release", PublishedToneKind::kongPackage, "tone137-kong-oboe-2.fytonepack" },
    { "violin-soft", "小提琴1", "violin", "natural", PublishedToneKind::swamPackage, "violin-soft.fytonepack" },
    { "kong-violin-2", "小提琴2", "container:kong-v3:khka_strings", "release", PublishedToneKind::kongPackage, "tone137-kong-violin-2.fytonepack" },
    { "cello", "大提琴", "cello", "natural", PublishedToneKind::swamPackage, "cello.fytonepack" },
    { "kong-erhu", "二胡1", "kong-erhu-2", "release", PublishedToneKind::kongPackage, "folk-erhu.fytonepack" },
    { "kong-erhu-new-2", "二胡2", "kong-erhu", "release", PublishedToneKind::kongPackage, "tone137-kong-erhu-new-2.fytonepack" },
    { "kong-hulusi-straight", "葫芦丝-直音", "kong-hulusi", "release", PublishedToneKind::kongPackage, "tone137-kong-hulusi-straight.fytonepack" },
    { "kong-hulusi-vibrato", "葫芦丝-颤音", "kong-hulusi", "release", PublishedToneKind::kongPackage, "tone137-kong-hulusi-vibrato.fytonepack" },
    { "kong-liuqin", "柳琴", "kong-liuqin", "release", PublishedToneKind::kongPackage, "folk-liuqin.fytonepack" },
    { "kong-qudi-straight", "曲笛1", "container:kong-v3:winds", "release", PublishedToneKind::kongPackage, "tone137-kong-qudi-straight.fytonepack" },
    { "kong-qudi", "曲笛2*", "kong-dizi", "release", PublishedToneKind::kongPackage, "folk-qudi.fytonepack" },
    { "kong-xun", "埙", "kong-xun", "release", PublishedToneKind::kongPackage, "tone137-kong-xun.fytonepack" },
    { "kong-suona-vibrato", "唢呐1", "kong-suona", "release", PublishedToneKind::kongPackage, "tone137-kong-suona-vibrato.fytonepack" },
    { "kong-suona", "唢呐2*", "kong-suona-2", "release", PublishedToneKind::kongPackage, "folk-suona.fytonepack" },
    { "kong-sanxian", "三弦", "kong-sanxian", "release", PublishedToneKind::kongPackage, "folk-sanxian.fytonepack" },
    { "kong-pipa", "琵琶", "kong-pipa", "release", PublishedToneKind::kongPackage, "folk-pipa.fytonepack" },
    { "kong-nanxiao", "南箫", "kong-nanxiao", "release", PublishedToneKind::kongPackage, "tone137-kong-nanxiao.fytonepack" },
    { "kong-dongxiao", "洞箫", "container:kong-v3:winds", "release", PublishedToneKind::kongPackage, "tone137-kong-dongxiao.fytonepack" },
    { "kong-bangdi", "梆笛*", "kong-dizi-2", "release", PublishedToneKind::kongPackage, "folk-bangdi.fytonepack" },
    { "kong-jinghu", "京胡", "kong-jinghu", "release", PublishedToneKind::kongPackage, "folk-jinghu.fytonepack" },
    { "kontakt-kensa", "肯萨1", "kontakt-kensa", "release", PublishedToneKind::kontaktPackage, "kontakt-kensa.fytonepack" },
    { "kontakt-kensa-2", "肯萨2", "kontakt-kensa", "release", PublishedToneKind::kontaktPackage, "tone137-kontakt-kensa-2.fytonepack" },
}};
}
