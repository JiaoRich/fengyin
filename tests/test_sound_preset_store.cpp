#include "SoundPresetStore.h"
#include <cassert>
#include <cmath>

int main()
{
    const auto directory = juce::File::getSpecialLocation(juce::File::tempDirectory)
        .getNonexistentChildFile("fengyin-preset-test", {}, true);
    directory.createDirectory();

    fengyin::SoundPresetStore store(directory);
    fengyin::SoundPreset preset;
    preset.id = "test-id";
    preset.name = "Alto Sax";
    preset.pluginIdentifier = "VST3-test-plugin";
    preset.effectIdentifier = "VST3-test-reverb";
    preset.breathController = 11;
    preset.breathCurve = 1.25f;
    const char state[] = { 1, 2, 3, 4, 5 };
    preset.pluginState.append(state, sizeof(state));
    preset.effectState.append(state, sizeof(state));
    preset.effectBypassed = true;
    preset.favorite = true;
    preset.eqTone = -0.12f;
    preset.warmth = 0.64f;
    preset.reverbMix = 0.42f;
    preset.toneStyleId = "warm-jazz";
    preset.baseToneStyleId = "warm-jazz";
    preset.pluginBrand = "kong";
    preset.instrumentKey = "kong-erhu";
    preset.instrumentChineseName = juce::String::fromUTF8("二胡");
    preset.pluginProgramName = "Erhu";
    preset.customTone = true;
    preset.compressionRatio = 2.4f;
    preset.harshControl = 0.22f;
    preset.outputGain = 0.92f;
    fengyin::TechniqueMapping technique;
    technique.technique = fengyin::PerformanceTechnique::growl;
    technique.sourceType = fengyin::TechniqueSourceType::controller;
    technique.sourceNumber = 21;
    technique.toggle = true;
    preset.techniqueMappings.add(technique);

    assert(store.save(preset));
    const auto loaded = store.findById("test-id");
    assert(loaded.has_value());
    assert(loaded->name == "Alto Sax");
    assert(loaded->breathController == 11);
    assert(std::abs(loaded->breathCurve - 1.25f) < 0.001f);
    assert(loaded->pluginState == preset.pluginState);
    assert(loaded->effectIdentifier == "VST3-test-reverb");
    assert(loaded->effectState == preset.effectState);
    assert(loaded->effectBypassed);
    assert(loaded->favorite);
    assert(std::abs(loaded->eqTone + 0.12f) < 0.001f);
    assert(std::abs(loaded->warmth - 0.64f) < 0.001f);
    assert(std::abs(loaded->reverbMix - 0.42f) < 0.001f);
    assert(loaded->toneStyleId == "warm-jazz");
    assert(loaded->baseToneStyleId == "warm-jazz");
    assert(loaded->pluginBrand == "kong");
    assert(loaded->instrumentKey == "kong-erhu");
    assert(loaded->instrumentChineseName == juce::String::fromUTF8("二胡"));
    assert(loaded->pluginProgramName == "Erhu");
    assert(loaded->customTone);
    assert(std::abs(loaded->compressionRatio - 2.4f) < 0.001f);
    assert(std::abs(loaded->harshControl - 0.22f) < 0.001f);
    assert(std::abs(loaded->outputGain - 0.92f) < 0.001f);
    assert(loaded->techniqueMappings.size() == 1);
    assert(loaded->techniqueMappings[0].sourceNumber == 21);
    assert(loaded->techniqueMappings[0].toggle);
    assert(store.setDefaultId("test-id"));
    assert(store.getDefaultId() == "test-id");

    preset.name = "Alto Sax Updated";
    assert(store.save(preset));
    assert(store.loadAll().size() == 1);
    assert(store.findById("test-id")->name == "Alto Sax Updated");
    assert(store.setFavorite("test-id", false));
    assert(! store.findById("test-id")->favorite);
    assert(store.remove("test-id"));
    assert(store.loadAll().isEmpty());

    directory.deleteRecursively();
}
