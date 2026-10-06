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
    const auto expected = key == "alto-sax" ? 3 : (key == "soprano-sax" || key == "tenor-sax" ? 2 : 1);
    assert(styles.size() == expected);
    for (const auto& style : styles) assert(style.settings.outputGain >= 1.0f);
    if (expected == 1) assert(styles[0].name == juce::String::fromUTF8("自然原声"));
    for (int i=1;i<styles.size();++i) assert(styles[i].id != styles[i-1].id);
}

void checkSaxophoneSwamProfiles(const juce::String& key)
{
    const auto styles = fengyin::ToneStyleCatalog::forInstrument(key);
    if (key == "baritone-sax") { assert(styles.size()==1); return; }
    for (const auto& style : styles) {
        assert(style.swam.enabled && !style.swam.releaseParameters.isEmpty());
        assert(style.swam.releaseModel.isNotEmpty());
        for (const auto& p:style.swam.releaseParameters) assert(p.value>=0 && p.value<=1);
    }
}
}

int main()
{
    // Fixed performance gain must preserve weak-breath dynamics and silence.
    fengyin::MasterOutputService quiet, louder;
    for (auto* chain : { &quiet, &louder }) {
        auto settings = chain->getToneStyle();
        settings.reverbMix = settings.saturation = settings.harshControl = settings.warmth = 0;
        chain->setToneStyle(settings);
        chain->setSmartOptimisationEnabled(false);
        chain->setSampleRate(48000);
    }
    float a[128]{}, b[128]{};
    float* qa[]{a}; float* qb[]{b};
    quiet.processInstrument(qa, 1, 128);
    louder.processInstrument(qb, 1, 128);
    for (float x : a) assert(x == 0);
    double power = 0;
    for (int block = 0; block < 80; ++block) {
        for (int i=0;i<128;++i) { a[i]=0.01f*std::sin((block*128+i)*0.1f); b[i]=2*a[i]; }
        quiet.processInstrument(qa, 1, 128);
        louder.processInstrument(qb, 1, 128);
        for (int i=0;i<128;++i) {
            assert(std::isfinite(a[i]) && std::isfinite(b[i]));
            assert(std::abs(b[i]-2*a[i]) < 0.00001f);
            power += a[i]*a[i];
        }
    }
    assert(power > 0);
    assert(quiet.getInstrumentPeak() > 0.0f);
    const auto instrumentPeak = quiet.getInstrumentPeak();
    // Browser/accompaniment/master-only processing must not drive the tone meter.
    for (auto& sample : a) sample = 0.8f;
    quiet.processMaster(qa, 1, 128);
    assert(quiet.getInstrumentPeak() == instrumentPeak);
    // Real instrument chain receives Air, while the accompaniment/master path
    // remains unchanged. Existing preset aggregate defaults keep Air disabled.
    assert(fengyin::ToneStyleCatalog::forInstrument("soprano-sax")[0].settings.air == 0.0f);
    fengyin::MasterOutputService dryChain, airChain;
    auto fx = dryChain.getToneStyle();
    fx.reverbMix = 0.0f;
    dryChain.setToneStyle(fx);
    fx.air = 0.8f;
    airChain.setToneStyle(fx);
    assert(airChain.getToneStyle().air == 0.8f);
    dryChain.setSampleRate(48000.0);
    airChain.setSampleRate(48000.0);
    float dry[128], wet[128];
    double difference = 0;
    for (int block = 0; block < 100; ++block)
    {
        for (int i = 0; i < 128; ++i)
            dry[i] = wet[i] = 0.03f * std::sin(static_cast<float>(block * 128 + i) * 1.2f);
        float* d[] { dry }; float* w[] { wet };
        dryChain.processInstrument(d, 1, 128);
        airChain.processInstrument(w, 1, 128);
        for (int i = 0; i < 128; ++i) difference += std::abs(dry[i] - wet[i]);
    }
    assert(difference > 1.0);
    for (int i = 0; i < 128; ++i) dry[i] = wet[i] = 0.2f;
    float* masterDry[] { dry }; float* masterWet[] { wet };
    dryChain.processMaster(masterDry, 1, 128);
    airChain.processMaster(masterWet, 1, 128);
    for (int i = 0; i < 128; ++i) assert(dry[i] == wet[i]);

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
