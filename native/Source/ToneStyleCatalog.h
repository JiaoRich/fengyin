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
        if (key == "violin" || key == "viola" || key == "cello" || key == "double-bass") return {
            natural(juce::String::fromUTF8("自然独奏")),
            { "warm", juce::String::fromUTF8("温暖抒情"), juce::String::fromUTF8("柔和圆润，适合慢歌旋律"), { -.16f,.56f,.34f,.52f,1.9f,.12f,.17f,.56f,.64f,.96f } },
            { "cinematic", juce::String::fromUTF8("电影叙事"), juce::String::fromUTF8("宽广、明亮、具有画面感"), { .18f,.38f,.40f,.48f,2.0f,.08f,.13f,.68f,.55f,1.0f } } };
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
