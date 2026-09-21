#pragma once

#include "MasterOutputService.h"
#include <juce_core/juce_core.h>

namespace fengyin
{
struct ToneStyleDefinition
{
    juce::String id;
    juce::String name;
    juce::String description;
    ToneStyleSettings settings;
};

class ToneStyleCatalog
{
public:
    static juce::Array<ToneStyleDefinition> forInstrument(const juce::String& key)
    {
        const auto natural = [](juce::String name = juce::String::fromUTF8("自然原声"))
        {
            return ToneStyleDefinition { "natural", name, juce::String::fromUTF8("真实、均衡、保留原始动态"),
                { 0.0f, .20f, .18f, .58f, 1.65f, .035f, .07f, .42f, .56f, .88f } };
        };
        if (key == "soprano-sax") return {
            natural(),
            { "silky", juce::String::fromUTF8("丝滑抒情"), juce::String::fromUTF8("温暖、柔和、浪漫大厅"), { -.12f,.58f,.36f,.52f,1.9f,.11f,.16f,.58f,.62f,.96f } },
            { "stage", juce::String::fromUTF8("明亮舞台"), juce::String::fromUTF8("清晰、明亮、更有穿透力"), { .28f,.30f,.24f,.50f,2.1f,.07f,.12f,.44f,.55f,.90f } } };
        if (key == "alto-sax") return {
            natural(),
            { "warm-jazz", juce::String::fromUTF8("温暖爵士"), juce::String::fromUTF8("厚实、松弛、带轻微暖色"), { -.18f,.66f,.25f,.50f,2.0f,.14f,.17f,.48f,.64f,.90f } },
            { "pop", juce::String::fromUTF8("流行穿透"), juce::String::fromUTF8("结实明快，容易融入伴奏"), { .30f,.32f,.20f,.48f,2.2f,.08f,.13f,.40f,.56f,.88f } } };
        if (key == "tenor-sax" || key == "baritone-sax") return {
            natural(),
            { "smoky", juce::String::fromUTF8("烟熏爵士"), juce::String::fromUTF8("低沉、温暖、略带粗粝感"), { -.24f,.74f,.23f,.50f,2.0f,.18f,.18f,.46f,.68f,.88f } },
            { "lyrical", juce::String::fromUTF8("深情抒情"), juce::String::fromUTF8("圆润、舒展、柔和大厅"), { -.10f,.60f,.34f,.53f,1.85f,.12f,.14f,.56f,.63f,.96f } } };
        if (key.contains("trumpet") || key.contains("flugelhorn")) return {
            natural(),
            { "bright-pop", juce::String::fromUTF8("明亮流行"), juce::String::fromUTF8("有冲击力，适合舞台与流行"), { .32f,.28f,.18f,.46f,2.25f,.07f,.20f,.38f,.58f,.86f } },
            { "soft", juce::String::fromUTF8("柔和抒情"), juce::String::fromUTF8("收敛刺耳感，温暖耐听"), { -.20f,.52f,.32f,.52f,1.9f,.10f,.24f,.54f,.66f,.94f } } };
        if (key.contains("flute") || key == "piccolo") return {
            natural(),
            { "clear", juce::String::fromUTF8("通透明亮"), juce::String::fromUTF8("清澈通透，适合轻快旋律"), { .25f,.10f,.22f,.56f,1.65f,.025f,.10f,.46f,.52f,.92f } },
            { "airy", juce::String::fromUTF8("空灵抒情"), juce::String::fromUTF8("气息感更强，空间更宽广"), { -.08f,.36f,.42f,.58f,1.55f,.06f,.12f,.66f,.58f,1.0f } } };
        if (key.contains("trombone") || key.contains("tuba") || key == "euphonium"
            || key.contains("horn")) return {
            natural(),
            { "warm-orchestral", juce::String::fromUTF8("温暖交响"), juce::String::fromUTF8("厚实、圆润、融入乐团"), { -.18f,.58f,.27f,.50f,2.0f,.13f,.21f,.52f,.66f,.92f } },
            { "cinematic-brass", juce::String::fromUTF8("电影史诗"), juce::String::fromUTF8("宽广、雄浑、富有力量"), { .15f,.34f,.38f,.45f,2.3f,.10f,.18f,.68f,.55f,1.0f } } };
        if (key.contains("clarinet") || key == "oboe" || key == "english-horn"
            || key.contains("bassoon")) return {
            natural(),
            { "warm-lyrical", juce::String::fromUTF8("温暖抒情"), juce::String::fromUTF8("柔和、圆润、适合歌唱性旋律"), { -.16f,.52f,.31f,.54f,1.75f,.08f,.16f,.55f,.64f,.94f } },
            { "cinematic-wood", juce::String::fromUTF8("电影叙事"), juce::String::fromUTF8("清晰而宽广，富有画面感"), { .12f,.34f,.39f,.51f,1.85f,.06f,.13f,.65f,.57f,1.0f } } };
        if (key == "violin" || key == "viola" || key == "cello" || key == "double-bass") return {
            natural(juce::String::fromUTF8("自然独奏")),
            { "warm", juce::String::fromUTF8("温暖抒情"), juce::String::fromUTF8("柔和圆润，适合慢歌旋律"), { -.16f,.56f,.34f,.52f,1.9f,.12f,.17f,.56f,.64f,.96f } },
            { "cinematic", juce::String::fromUTF8("电影叙事"), juce::String::fromUTF8("宽广、明亮、具有画面感"), { .18f,.38f,.40f,.48f,2.0f,.08f,.13f,.68f,.55f,1.0f } } };
        if (key.startsWith("kong-"))
        {
            if (key == "kong-dizi" || key == "kong-xiao" || key == "kong-xun") return {
                natural(),
                { "silk-bamboo", juce::String::fromUTF8("清雅丝竹"), juce::String::fromUTF8("清透自然，保留气息细节"), { .12f,.28f,.29f,.57f,1.55f,.035f,.11f,.52f,.60f,.96f } },
                { "landscape", juce::String::fromUTF8("空灵山水"), juce::String::fromUTF8("宽广悠远，适合古风抒情"), { -.10f,.42f,.44f,.60f,1.50f,.05f,.12f,.70f,.62f,1.0f } } };
            if (key == "kong-suona" || key == "kong-guanzi") return {
                natural(),
                { "festive", juce::String::fromUTF8("喜庆明亮"), juce::String::fromUTF8("鲜明通透，保留民乐冲击力"), { .24f,.20f,.19f,.46f,2.25f,.08f,.23f,.40f,.58f,.88f } },
                { "epic-folk", juce::String::fromUTF8("厚重叙事"), juce::String::fromUTF8("稳重有力，适合大场面"), { -.08f,.46f,.35f,.47f,2.2f,.12f,.20f,.62f,.58f,.98f } } };
            return {
                natural(),
                { "warm-folk", juce::String::fromUTF8("温润抒情"), juce::String::fromUTF8("柔和耐听，适合慢速旋律"), { -.15f,.54f,.30f,.54f,1.75f,.08f,.14f,.54f,.64f,.94f } },
                { "cinematic-folk", juce::String::fromUTF8("国风叙事"), juce::String::fromUTF8("宽广清晰，适合古风与影视"), { .10f,.36f,.39f,.50f,1.9f,.07f,.13f,.66f,.57f,1.0f } } };
        }
        return { natural() };
    }

    static ToneStyleDefinition find(const juce::String& instrumentKey, const juce::String& styleId)
    {
        const auto styles = forInstrument(instrumentKey);
        for (const auto& style : styles) if (style.id == styleId) return style;
        return styles.getFirst();
    }
};
}
