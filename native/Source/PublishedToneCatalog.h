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
inline constexpr std::array<PublishedToneDefinition, 73> publishedToneCatalog {{
    { "kontakt-kensa", "肯萨", "kontakt-kensa", "release", PublishedToneKind::kontaktPackage, "kontakt-kensa.fytonepack" },
    { "soprano-kenny", "原声高萨", "soprano-sax", "kenny", PublishedToneKind::swamPackage, "soprano-kenny.fytonepack" },
    { "soprano-pop", "明亮高萨", "soprano-sax", "pop-high", PublishedToneKind::swamPackage, "soprano-pop.fytonepack" },
    { "dj-source-02", "DJ · 高萨", "soprano-sax", "dj-source", PublishedToneKind::swamPackage, "dj-source-02.fytonepack" },
    { "dj-source-03", "DJ · 高萨2", "soprano-sax", "dj-source", PublishedToneKind::swamPackage, "dj-source-03.fytonepack" },
    { "alto-jazz", "爵士中萨", "alto-sax", "jazz", PublishedToneKind::swamPackage, "alto-jazz.fytonepack" },
    { "alto-deep", "深情中萨", "alto-sax", "deep", PublishedToneKind::swamPackage, "alto-deep.fytonepack" },
    { "alto-pop", "流行中萨", "alto-sax", "pop", PublishedToneKind::swamPackage, "alto-pop.fytonepack" },
    { "alto-silky", "浪漫中萨", "alto-sax", "release", PublishedToneKind::swamPackage, "tone137-alto-silky.fytonepack" },
    { "alto-breath-growl", "嘶吼中萨", "alto-sax", "release", PublishedToneKind::swamPackage, "alto-breath-growl.fytonepack" },
    { "alto-breathy", "气声中萨", "alto-sax", "release", PublishedToneKind::swamPackage, "alto-breathy.fytonepack" },
    { "dj-source-04", "DJ · 中音萨克斯", "alto-sax", "dj-source", PublishedToneKind::swamPackage, "dj-source-04.fytonepack" },
    { "dj-source-05", "DJ · 中音明亮萨克斯", "alto-sax", "dj-source", PublishedToneKind::swamPackage, "dj-source-05.fytonepack" },
    { "dj-source-06", "DJ · 中音温暖萨克斯", "alto-sax", "dj-source", PublishedToneKind::swamPackage, "dj-source-06.fytonepack" },
    { "dj-source-07", "DJ · 中音顺滑萨克斯", "alto-sax", "dj-source", PublishedToneKind::swamPackage, "dj-source-07.fytonepack" },
    { "dj-source-08", "DJ · 流行中音萨克斯", "alto-sax", "dj-source", PublishedToneKind::swamPackage, "dj-source-08.fytonepack" },
    { "dj-source-09", "DJ · 中音爵士萨克斯", "alto-sax", "dj-source", PublishedToneKind::swamPackage, "dj-source-09.fytonepack" },
    { "tenor-mellow", "醇厚次中萨", "tenor-sax", "mellow", PublishedToneKind::swamPackage, "tenor-mellow.fytonepack" },
    { "tenor-warm", "温暖次中萨", "tenor-sax", "warm", PublishedToneKind::swamPackage, "tenor-warm.fytonepack" },
    { "tenor-air-pocket", "气包音次中萨", "tenor-sax", "air-pocket", PublishedToneKind::swamPackage, "tenor-air-pocket.fytonepack" },
    { "tenor-silky", "浪漫次中萨", "tenor-sax", "release", PublishedToneKind::swamPackage, "tone137-tenor-silky.fytonepack" },
    { "tenor-breath-growl", "嘶吼次中萨", "tenor-sax", "release", PublishedToneKind::swamPackage, "tenor-breath-growl.fytonepack" },
    { "dj-source-10", "DJ · 次中音萨克斯", "tenor-sax", "dj-source", PublishedToneKind::swamPackage, "dj-source-10.fytonepack" },
    { "dj-source-11", "DJ · 次中音明亮萨克斯", "tenor-sax", "dj-source", PublishedToneKind::swamPackage, "dj-source-11.fytonepack" },
    { "dj-source-12", "DJ · 次中音温暖萨克斯", "tenor-sax", "dj-source", PublishedToneKind::swamPackage, "dj-source-12.fytonepack" },
    { "dj-source-13", "DJ · 次中音顺滑萨克斯", "tenor-sax", "dj-source", PublishedToneKind::swamPackage, "dj-source-13.fytonepack" },
    { "dj-source-14", "DJ · 次中音特定萨克斯", "tenor-sax", "dj-source", PublishedToneKind::swamPackage, "dj-source-14.fytonepack" },
    { "dj-source-15", "DJ · 次中音克特兰萨克斯", "tenor-sax", "dj-source", PublishedToneKind::swamPackage, "dj-source-15.fytonepack" },
    { "dj-source-16", "DJ · 次中气包音萨克斯", "tenor-sax", "dj-source", PublishedToneKind::swamPackage, "dj-source-16.fytonepack" },
    { "dj-source-17", "DJ · 次中气包明亮萨克斯", "tenor-sax", "dj-source", PublishedToneKind::swamPackage, "dj-source-17.fytonepack" },
    { "dj-source-18", "DJ · 次中气包特定萨克斯", "tenor-sax", "dj-source", PublishedToneKind::swamPackage, "dj-source-18.fytonepack" },
    { "dj-source-19", "DJ · 低音萨克斯", "baritone-sax", "dj-source", PublishedToneKind::swamPackage, "dj-source-19.fytonepack" },
    { "dj-source-20", "DJ · 低音顺滑萨克斯", "baritone-sax", "dj-source", PublishedToneKind::swamPackage, "dj-source-20.fytonepack" },
    { "trumpet", "小号1", "trumpet", "natural", PublishedToneKind::swamPackage, "trumpet.fytonepack" },
    { "trumpet-2", "小号2", "trumpet", "release", PublishedToneKind::swamPackage, "tone137-trumpet-2.fytonepack" },
    { "dj-source-24", "DJ · 小号1", "trumpet", "dj-source", PublishedToneKind::swamPackage, "dj-source-24.fytonepack" },
    { "piccolo-trumpet", "高音小号", "piccolo-trumpet", "natural", PublishedToneKind::swamPackage, "piccolo-trumpet.fytonepack" },
    { "alto-trombone", "中音长号", "alto-trombone", "natural", PublishedToneKind::swamPackage, "alto-trombone.fytonepack" },
    { "bass-trombone", "低音长号", "bass-trombone", "natural", PublishedToneKind::swamPackage, "bass-trombone.fytonepack" },
    { "flute", "长笛", "flute", "natural", PublishedToneKind::swamPackage, "flute.fytonepack" },
    { "dj-source-23", "DJ · 长笛", "flute", "dj-source", PublishedToneKind::swamPackage, "dj-source-23.fytonepack" },
    { "piccolo", "短笛", "piccolo", "natural", PublishedToneKind::swamPackage, "piccolo.fytonepack" },
    { "clarinet", "单簧管1", "clarinet", "natural", PublishedToneKind::swamPackage, "clarinet.fytonepack" },
    { "dj-source-22", "DJ · 单簧管", "clarinet", "dj-source", PublishedToneKind::swamPackage, "dj-source-22.fytonepack" },
    { "kong-clarinet-2", "单簧管2", "container:kong-v3:khka_woodwinds", "release", PublishedToneKind::kongPackage, "tone137-kong-clarinet-2.fytonepack" },
    { "oboe", "双簧管1", "oboe", "natural", PublishedToneKind::swamPackage, "oboe.fytonepack" },
    { "dj-source-21", "DJ · 双簧管", "oboe", "dj-source", PublishedToneKind::swamPackage, "dj-source-21.fytonepack" },
    { "kong-oboe-2", "双簧管2", "container:kong-v3:khka_woodwinds", "release", PublishedToneKind::kongPackage, "tone137-kong-oboe-2.fytonepack" },
    { "violin-soft", "小提琴1", "violin", "natural", PublishedToneKind::swamPackage, "violin-soft.fytonepack" },
    { "dj-source-26", "DJ · 小提琴", "violin", "dj-source", PublishedToneKind::swamPackage, "dj-source-26.fytonepack" },
    { "dj-source-27", "DJ · 小提琴环绕", "violin", "dj-source", PublishedToneKind::swamPackage, "dj-source-27.fytonepack" },
    { "kong-violin-2", "小提琴2", "container:kong-v3:khka_strings", "release", PublishedToneKind::kongPackage, "tone137-kong-violin-2.fytonepack" },
    { "cello", "大提琴", "cello", "natural", PublishedToneKind::swamPackage, "cello.fytonepack" },
    { "dj-source-28", "DJ · 大提琴", "cello", "dj-source", PublishedToneKind::swamPackage, "dj-source-28.fytonepack" },
    { "dj-source-35", "DJ · 马头琴", "cello", "dj-source", PublishedToneKind::swamPackage, "dj-source-35.fytonepack" },
    { "kong-erhu", "二胡1", "kong-erhu-2", "release", PublishedToneKind::kongPackage, "folk-erhu.fytonepack" },
    { "kong-erhu-new-2", "二胡2", "kong-erhu", "release", PublishedToneKind::kongPackage, "tone137-kong-erhu-new-2.fytonepack" },
    { "kong-jinghu", "京胡", "kong-jinghu", "release", PublishedToneKind::kongPackage, "folk-jinghu.fytonepack" },
    { "kong-hulusi-straight", "葫芦丝-直音", "kong-hulusi", "release", PublishedToneKind::kongPackage, "tone137-kong-hulusi-straight.fytonepack" },
    { "kong-hulusi-vibrato", "葫芦丝-颤音", "kong-hulusi", "release", PublishedToneKind::kongPackage, "tone137-kong-hulusi-vibrato.fytonepack" },
    { "kong-qudi-straight", "曲笛1", "container:kong-v3:winds", "release", PublishedToneKind::kongPackage, "tone137-kong-qudi-straight.fytonepack" },
    { "kong-qudi", "曲笛2*", "kong-dizi", "release", PublishedToneKind::kongPackage, "folk-qudi.fytonepack" },
    { "kong-bangdi", "梆笛*", "kong-dizi-2", "release", PublishedToneKind::kongPackage, "folk-bangdi.fytonepack" },
    { "kong-nanxiao", "南箫", "kong-nanxiao", "release", PublishedToneKind::kongPackage, "tone137-kong-nanxiao.fytonepack" },
    { "kong-dongxiao", "洞箫", "container:kong-v3:winds", "release", PublishedToneKind::kongPackage, "tone137-kong-dongxiao.fytonepack" },
    { "dj-source-33", "DJ · 箫1", "alto-flute", "dj-source", PublishedToneKind::swamPackage, "dj-source-33.fytonepack" },
    { "dj-source-34", "DJ · 箫2", "bass-flute", "dj-source", PublishedToneKind::swamPackage, "dj-source-34.fytonepack" },
    { "kong-xun", "埙", "kong-xun", "release", PublishedToneKind::kongPackage, "tone137-kong-xun.fytonepack" },
    { "kong-suona-vibrato", "唢呐1", "kong-suona", "release", PublishedToneKind::kongPackage, "tone137-kong-suona-vibrato.fytonepack" },
    { "kong-suona", "唢呐2*", "kong-suona-2", "release", PublishedToneKind::kongPackage, "folk-suona.fytonepack" },
    { "kong-liuqin", "柳琴", "kong-liuqin", "release", PublishedToneKind::kongPackage, "folk-liuqin.fytonepack" },
    { "kong-sanxian", "三弦", "kong-sanxian", "release", PublishedToneKind::kongPackage, "folk-sanxian.fytonepack" },
    { "kong-pipa", "琵琶", "kong-pipa", "release", PublishedToneKind::kongPackage, "folk-pipa.fytonepack" },
}};
}
