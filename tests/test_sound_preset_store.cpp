#include "SoundPresetStore.h"
#include "KongProjectFile.h"
#include "KongReleaseStates.h"
#include "KongTechniqueMap.h"
#include <cassert>
#include <cmath>

int main()
{
    const auto releaseFile = juce::File(juce::String::fromUTF8(__FILE__)).getParentDirectory().getParentDirectory()
        .getChildFile("assets/tone-packages/kong/release-states.xml");
    const auto releaseXml = releaseFile.loadFileAsString();
    auto releaseRoot = juce::parseXML(releaseXml);
    assert(releaseRoot && releaseRoot->getNumChildElements() == 12);
    for (const auto* asset : {"erhu.kam", "guzheng.kam", "hulusi.kam", "liuqin.kam",
                             "matouqin.kam", "qudi.kam", "xun.kam", "suona.kam",
                             "sanxian.kam", "pipa.kam", "sheng.kam", "nanxiao.kam"})
    {
        const auto state = fengyin::KongReleaseStates::read(releaseXml, asset);
        assert(state.getSize() > 1000);
        auto wrapper = juce::parseXML(juce::String::fromUTF8(static_cast<const char*>(state.getData()) + 8,
            static_cast<int>(juce::ByteOrder::littleEndianInt(static_cast<const char*>(state.getData()) + 4))));
        juce::MemoryBlock component;
        assert(component.fromBase64Encoding(wrapper->getChildByName("IComponent")->getAllSubText()));
        const auto tree = juce::ValueTree::readFromData(component.getData(), component.getSize());
        struct Expected { const char* asset; const char* technique; int note; };
        for (const auto& expected : {Expected{"erhu.kam","Vib_Main",26}, {"guzheng.kam","Main_Shake_Mellow",28},
            {"hulusi.kam","Vib_Main",26}, {"liuqin.kam","Roll",30}, {"matouqin.kam","Pizz",32},
            {"qudi.kam","Vib_Main",29}, {"xun.kam","Vib_Main",27}, {"suona.kam","Flutter",35},
            {"sanxian.kam","Sus_Main_Roll",32}, {"pipa.kam","Roll",32}, {"sheng.kam","Flutter",32},
            {"nanxiao.kam","Vib_Main",30}})
            if (juce::String(asset) == expected.asset)
            {
                const auto binding = fengyin::findKongTechnique(tree, {expected.technique});
                assert(binding.keyswitch == expected.note);
                assert(binding.channel == 1);
                assert(binding.normalKey == 24);
            }
        assert(fengyin::findKongTechnique(tree, {"nonexistent-articulation"}).keyswitch == -1);
        bool compared = false;
        for (auto* entry : releaseRoot->getChildIterator())
            if (entry->getStringAttribute("asset") == asset)
            {
                juce::MemoryBlock original;
                assert(original.fromBase64Encoding(entry->getAllSubText()));
                assert(state == original); // Keep every slot/channel/keyswitch byte.
                compared = true;
            }
        assert(compared);
    }
    assert(fengyin::KongReleaseStates::read(releaseXml, "missing.kam").isEmpty());
    assert(fengyin::KongReleaseStates::read("<invalid/>", "erhu.kam").isEmpty());
    const auto directory = juce::File::getSpecialLocation(juce::File::tempDirectory)
        .getNonexistentChildFile("fengyin-preset-test", {}, true);
    directory.createDirectory();

    fengyin::SoundPresetStore store(directory);
    fengyin::SoundPreset preset;
    preset.id = "test-id";
    preset.name = "Alto Sax";
    preset.pluginIdentifier = "VST3-test-plugin";
    preset.instrumentModelIndex = 2;
    preset.toneParameters.add({ "id:timbre", 0.72f });
    preset.toneParameters.add({ "id:brightness", 0.44f });
    preset.eqTone = -0.12f;
    preset.warmth = 0.64f;
    preset.reverbMix = 0.42f;
    preset.toneStyleId = "warm-jazz";
    preset.baseToneStyleId = "warm-jazz";
    preset.pluginBrand = "kong";
    preset.instrumentKey = "kong-erhu";
    preset.instrumentChineseName = juce::String::fromUTF8("二胡");
    preset.pluginProgramName = "Erhu";
    preset.containerInstrument = true;
    preset.containerAdapter = "kong-v3";
    const unsigned char samplerBytes[] { 0, 1, 127, 128, 255, 0, 42 };
    preset.samplerState = juce::MemoryBlock(samplerBytes, sizeof(samplerBytes));
    preset.containerProjectState = fengyin::KongProjectFile::create("ErHu");
    // Matches the supplied QinEngine 3.11 二胡.KAM ValueTree payload size.
    assert(preset.containerProjectState.getSize() == 367);
    const auto referencePath = juce::SystemStats::getEnvironmentVariable("FENGYIN_REFERENCE_KAM", {});
    if (referencePath.isNotEmpty())
    {
        juce::MemoryBlock reference;
        assert(juce::File(referencePath).loadFileAsData(reference));
        assert(reference == preset.containerProjectState);
    }
    // Windows QinEngine builds may return the native KAM payload directly.
    // It must be accepted unchanged instead of failing creation.
    assert(fengyin::KongProjectFile::fromPluginState(preset.containerProjectState)
           == preset.containerProjectState);
    const auto wrappedVst3State = fengyin::KongProjectFile::toPluginState(preset.containerProjectState);
    assert(wrappedVst3State.getSize() > preset.containerProjectState.getSize());
    const auto wrapperLength = static_cast<int>(juce::ByteOrder::littleEndianInt(
        static_cast<const char*>(wrappedVst3State.getData()) + 4));
    auto wrapperXml = juce::parseXML(juce::String::fromUTF8(
        static_cast<const char*>(wrappedVst3State.getData()) + 8, wrapperLength));
    assert(wrapperXml != nullptr);
    juce::MemoryBlock componentState;
    assert(componentState.fromBase64Encoding(wrapperXml->getChildByName("IComponent")->getAllSubText()));
    const auto activeState = juce::ValueTree::readFromData(componentState.getData(), componentState.getSize());
    const auto activeSlot = activeState.getChildWithName("PresetList").getChild(0);
    assert(static_cast<bool>(activeSlot.getProperty("IsSelected", false)));
    const auto generatedRoundTrip = fengyin::KongProjectFile::fromPluginState(wrappedVst3State);
    const auto generatedDescription = fengyin::KongProjectFile::describe(generatedRoundTrip);
    assert(generatedDescription.has_value());
    assert(generatedDescription->kai == "ErHu");
    assert(generatedDescription->preset == "Sus_Main");
    const auto referenceVstStatePath = juce::SystemStats::getEnvironmentVariable("FENGYIN_REFERENCE_VST_STATE", {});
    if (referenceVstStatePath.isNotEmpty())
    {
        juce::MemoryBlock referenceState;
        assert(juce::File(referenceVstStatePath).loadFileAsData(referenceState));
        const auto extractedProject = fengyin::KongProjectFile::fromPluginState(referenceState);
        assert(extractedProject == preset.containerProjectState);
        const auto extracted = fengyin::KongProjectFile::describe(extractedProject);
        assert(extracted.has_value());
        assert(extracted->kai == "ErHu");
        assert(extracted->preset == "Sus_Main");
    }
    preset.customTone = true;
    preset.compressionRatio = 2.4f;
    preset.harshControl = 0.22f;
    preset.outputGain = 0.92f;
    preset.bass = -0.35f;
    preset.air = 0.67f;
    preset.studioDraft = true;
    preset.instrumentDescriptionXml = "<PLUGIN name=\"test\"/>";
    preset.instrumentState = juce::MemoryBlock(samplerBytes, sizeof(samplerBytes));
    preset.effects.push_back({"<PLUGIN name=\"EQ\"/>", juce::MemoryBlock(samplerBytes, sizeof(samplerBytes)), false});
    preset.effects.push_back({"<PLUGIN name=\"Compressor\"/>", juce::MemoryBlock(samplerBytes, 3), true});

    assert(store.save(preset));
    const auto loaded = store.findById("test-id");
    assert(loaded.has_value());
    assert(loaded->studioDraft);
    assert(loaded->instrumentState == preset.instrumentState);
    assert(loaded->instrumentDescriptionXml == preset.instrumentDescriptionXml);
    assert(loaded->effects.size() == 2);
    assert(loaded->effects[0].descriptionXml == preset.effects[0].descriptionXml);
    assert(loaded->effects[1].state == preset.effects[1].state);
    assert(!loaded->effects[0].bypassed && loaded->effects[1].bypassed);
    assert(loaded->name == "Alto Sax");
    assert(loaded->instrumentModelIndex == 2);
    assert(loaded->toneParameters.size() == 2);
    assert(loaded->toneParameters[0].identifier == "id:timbre");
    assert(std::abs(loaded->toneParameters[0].value - 0.72f) < 0.001f);
    assert(std::abs(loaded->eqTone + 0.12f) < 0.001f);
    assert(std::abs(loaded->warmth - 0.64f) < 0.001f);
    assert(std::abs(loaded->reverbMix - 0.42f) < 0.001f);
    assert(loaded->toneStyleId == "warm-jazz");
    assert(loaded->baseToneStyleId == "warm-jazz");
    assert(loaded->pluginBrand == "kong");
    assert(loaded->instrumentKey == "kong-erhu");
    assert(loaded->instrumentChineseName == juce::String::fromUTF8("二胡"));
    assert(loaded->pluginProgramName == "Erhu");
    assert(loaded->containerInstrument);
    assert(loaded->containerAdapter == "kong-v3");
    assert(loaded->samplerState == preset.samplerState);
    assert(loaded->containerProjectState == preset.containerProjectState);
    const auto project = fengyin::KongProjectFile::describe(loaded->containerProjectState);
    assert(project.has_value());
    assert(project->kai == "ErHu");
    assert(project->preset == "Sus_Main");
    assert(project->midiChannel == 0);
    assert(project->audioOutput == 0);
    const auto pluginState = fengyin::KongProjectFile::toPluginState(loaded->containerProjectState);
    assert(pluginState.getSize() > 0);
    const auto roundTripKam = fengyin::KongProjectFile::fromPluginState(pluginState);
    const auto roundTrip = fengyin::KongProjectFile::describe(roundTripKam);
    assert(roundTrip.has_value());
    assert(roundTrip->kai == "ErHu");
    assert(roundTrip->preset == "Sus_Main");
    assert(loaded->customTone);
    assert(std::abs(loaded->compressionRatio - 2.4f) < 0.001f);
    assert(std::abs(loaded->harshControl - 0.22f) < 0.001f);
    assert(std::abs(loaded->outputGain - 0.92f) < 0.001f);
    assert(std::abs(loaded->bass + 0.35f) < 0.001f);
    assert(std::abs(loaded->air - 0.67f) < 0.001f);
    assert(store.setDefaultId("test-id"));
    assert(store.getDefaultId() == "test-id");

    preset.name = "Alto Sax Updated";
    assert(store.save(preset));
    assert(store.loadAll().size() == 1);
    assert(store.findById("test-id")->name == "Alto Sax Updated");
    assert(store.remove("test-id"));
    assert(store.loadAll().isEmpty());
    preset.pluginBrand = "swam";
    preset.containerInstrument = false;
    preset.containerAdapter.clear();
    preset.containerProjectState.reset();
    assert(store.save(preset));
    assert(store.findById("test-id")->samplerState.getSize() == 0);
    assert(std::abs(store.findById("test-id")->air - 0.67f) < 0.001f);

    // A pre-Air preset must load as fully bypassed, not inherit the last tone.
    const auto presetFile = directory.getChildFile("sound-presets.xml");
    auto legacy = juce::XmlDocument::parse(presetFile);
    assert(legacy != nullptr);
    legacy->getFirstChildElement()->removeAttribute("air");
    assert(legacy->writeTo(presetFile));
    assert(store.findById("test-id")->air == 0.0f);

    auto malformed = fengyin::SoundPresetStore::toXml({preset});
    malformed->getFirstChildElement()->getChildByName("INSTRUMENT_STATE")->deleteAllTextElements();
    malformed->getFirstChildElement()->getChildByName("INSTRUMENT_STATE")->addTextElement("not a state");
    assert(fengyin::SoundPresetStore::fromXml(*malformed).isEmpty());
    auto tooMany = preset;
    tooMany.effects.resize(17);
    assert(!store.save(tooMany));
    directory.deleteRecursively();
}
