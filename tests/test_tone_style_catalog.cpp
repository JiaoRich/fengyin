#include "ToneStyleCatalog.h"
#include "KongInstrumentCatalog.h"
#include "KongLibraryLocator.h"
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

void checkSaxophoneSwamProfiles(const juce::String& key)
{
    const auto styles = fengyin::ToneStyleCatalog::forInstrument(key);
    assert(styles.size() == 3);
    for (const auto& style : styles) assert(style.swam.enabled);
    assert(std::abs(styles[0].swam.brightness - styles[1].swam.brightness) > 0.04f);
    assert(std::abs(styles[1].swam.timbre - styles[2].swam.timbre) > 0.04f);
}
}

int main()
{
    const auto temporary = juce::File::getSpecialLocation(juce::File::tempDirectory)
        .getNonexistentChildFile("fengyin-library-test", {}, true);
    const auto bank = temporary.getChildFile(juce::String::fromUTF8("用户 自选库"));
    assert(bank.createDirectory().wasOk());
    assert(bank.getChildFile("ErHu.KAI").replaceWithText("fixture"));
    assert(bank.getChildFile("BaWu.kai").replaceWithText("fixture"));
    assert(bank.getChildFile("ignore.txt").replaceWithText("fixture"));
    assert(fengyin::KongLibraryLocator::describe("ErHu_II.KAI").chineseName == juce::String::fromUTF8("二胡二"));
    assert(fengyin::KongLibraryLocator::describe("BianZhong_Pro.KAI").chineseName == juce::String::fromUTF8("专业编钟"));
    assert(fengyin::KongLibraryLocator::describe("BianQing_23.KAI").chineseName == juce::String::fromUTF8("编磬23"));
    assert(fengyin::KongLibraryLocator::describe("KeKeJiaoXiang_TongGuan.KAI").chineseName == juce::String::fromUTF8("柯克交响·铜管"));
    assert(fengyin::KongLibraryLocator::describe("My_New_Instrument.KAI").chineseName == "My New Instrument");
    assert(! fengyin::KongLibraryLocator::describe("My_New_Instrument.KAI").recognised);
    const auto choice = temporary.getChildFile("choice.txt");
    const auto config = temporary.getChildFile("config");
    const auto missing = temporary.getChildFile("missing");
    assert(!fengyin::KongLibraryLocator::resolve(choice,config,missing).ready());
    assert(choice.replaceWithText(bank.getFullPathName()));
    auto resolved = fengyin::KongLibraryLocator::resolve(choice,config,missing);
    assert(resolved.ready() && resolved.source == "selected" && resolved.files.size() == 2);
    assert(resolved.directory == bank);
    juce::XmlElement xml("KAConfigFile");
    xml.setAttribute("KAIFolderPath", missing.getFullPathName());
    assert(xml.writeTo(config));
    resolved = fengyin::KongLibraryLocator::resolve(choice,config,bank);
    assert(!resolved.ready() && resolved.source == "engine" && resolved.directory == missing);
    assert(config.replaceWithText("unrecognised binary config"));
    assert(choice.replaceWithText(missing.getFullPathName()));
    resolved = fengyin::KongLibraryLocator::resolve(choice,config,bank);
    assert(!resolved.ready() && resolved.source == "selected"); // Do not silently switch to another bank.
    assert(choice.deleteFile());
    resolved = fengyin::KongLibraryLocator::resolve(choice,config,bank);
    assert(resolved.ready() && resolved.source == "default");
    assert(temporary.deleteRecursively());
    const char* swamKeys[] { "soprano-sax", "alto-sax", "tenor-sax", "baritone-sax",
        "flugelhorn-eb", "flugelhorn", "piccolo-trumpet", "trumpet-c", "trumpet",
        "double-bass-trombone", "tenor-bass-trombone", "bass-trombone", "alto-trombone",
        "tenor-trombone", "bass-tuba", "tuba-eb", "euphonium", "horn-bb", "horn-f",
        "piccolo", "bass-flute", "alto-flute", "flute", "bass-clarinet", "clarinet",
        "english-horn", "oboe", "contrabassoon", "bassoon", "double-bass", "violin", "viola", "cello" };
    for (const auto* key : swamKeys)
        checkStyles(key);
    checkSaxophoneSwamProfiles("soprano-sax");
    checkSaxophoneSwamProfiles("alto-sax");
    checkSaxophoneSwamProfiles("tenor-sax");
    checkSaxophoneSwamProfiles("baritone-sax");
    for (const auto& instrument : fengyin::KongInstrumentCatalog::all())
        checkStyles(instrument.key);
    assert(fengyin::KongInstrumentCatalog::matchProgram("Qin Dizi Solo") != nullptr);
    assert(juce::String(fengyin::KongInstrumentCatalog::matchProgram("Dizi_2")->key) == "kong-dizi-2");
    assert(juce::String(fengyin::KongInstrumentCatalog::matchProgram("Erhu_2")->key) == "kong-erhu-2");
    assert(fengyin::KongInstrumentCatalog::matchProgram("Er Hu expressive") != nullptr);
    assert(fengyin::KongInstrumentCatalog::matchProgram("Unrelated Program") == nullptr);
    assert(fengyin::SupportedInstrumentClassifier::classify("", "",
        "C:\\Program Files\\Common Files\\VST3\\Kong Audio\\QinEngineV3.vst3")
        == fengyin::SupportedInstrumentClassifier::Brand::kong);
    assert(fengyin::SupportedInstrumentClassifier::classify("", "", "C:\\VST3\\Other.vst3")
        == fengyin::SupportedInstrumentClassifier::Brand::unsupported);
    assert(juce::String(fengyin::KongInstrumentCatalog::matchProgram(juce::String::fromUTF8("二胡二"))->key) == "kong-erhu-2");
    assert(fengyin::SupportedInstrumentClassifier::classify("Qin", juce::String::fromUTF8("空音"))
        == fengyin::SupportedInstrumentClassifier::Brand::kong);
}
