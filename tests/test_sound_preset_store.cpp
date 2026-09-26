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
    preset.customTone = true;
    preset.compressionRatio = 2.4f;
    preset.harshControl = 0.22f;
    preset.outputGain = 0.92f;
    preset.bass = -0.35f;

    assert(store.save(preset));
    const auto loaded = store.findById("test-id");
    assert(loaded.has_value());
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
    assert(loaded->customTone);
    assert(std::abs(loaded->compressionRatio - 2.4f) < 0.001f);
    assert(std::abs(loaded->harshControl - 0.22f) < 0.001f);
    assert(std::abs(loaded->outputGain - 0.92f) < 0.001f);
    assert(std::abs(loaded->bass + 0.35f) < 0.001f);
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
    assert(store.save(preset));
    assert(store.findById("test-id")->samplerState.getSize() == 0);

    directory.deleteRecursively();
}
