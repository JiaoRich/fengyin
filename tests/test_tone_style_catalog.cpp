#include "ToneStyleCatalog.h"
#include "KongInstrumentCatalog.h"
#include <cassert>
#include <cmath>

namespace
{
bool materiallyDifferent(const fengyin::ToneStyleSettings& first,
                         const fengyin::ToneStyleSettings& second)
{
    return std::abs(first.tone - second.tone) > 0.02f
        || std::abs(first.warmth - second.warmth) > 0.02f
        || std::abs(first.reverbMix - second.reverbMix) > 0.02f
        || std::abs(first.compressionThreshold - second.compressionThreshold) > 0.02f
        || std::abs(first.saturation - second.saturation) > 0.02f
        || std::abs(first.harshControl - second.harshControl) > 0.02f;
}

void checkStyles(const juce::String& key)
{
    const auto styles = fengyin::ToneStyleCatalog::forInstrument(key);
    assert(styles.size() == 3);
    assert(styles[0].id != styles[1].id && styles[1].id != styles[2].id);
    assert(materiallyDifferent(styles[0].settings, styles[1].settings));
    assert(materiallyDifferent(styles[1].settings, styles[2].settings));
}
}

int main()
{
    const char* swamKeys[] { "soprano-sax", "alto-sax", "tenor-sax", "baritone-sax",
        "flugelhorn-eb", "flugelhorn", "piccolo-trumpet", "trumpet-c", "trumpet",
        "double-bass-trombone", "tenor-bass-trombone", "bass-trombone", "alto-trombone",
        "tenor-trombone", "bass-tuba", "tuba-eb", "euphonium", "horn-bb", "horn-f",
        "piccolo", "bass-flute", "alto-flute", "flute", "bass-clarinet", "clarinet",
        "english-horn", "oboe", "contrabassoon", "bassoon", "double-bass", "violin", "viola", "cello" };
    for (const auto* key : swamKeys)
        checkStyles(key);
    for (const auto& instrument : fengyin::KongInstrumentCatalog::all())
        checkStyles(instrument.key);
    assert(fengyin::KongInstrumentCatalog::matchProgram("Qin Dizi Solo") != nullptr);
    assert(juce::String(fengyin::KongInstrumentCatalog::matchProgram("Dizi_2")->key) == "kong-dizi-2");
    assert(juce::String(fengyin::KongInstrumentCatalog::matchProgram("Erhu_2")->key) == "kong-erhu-2");
    assert(fengyin::KongInstrumentCatalog::matchProgram("Er Hu expressive") != nullptr);
    assert(fengyin::KongInstrumentCatalog::matchProgram("Unrelated Program") == nullptr);
}
