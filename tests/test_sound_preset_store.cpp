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
