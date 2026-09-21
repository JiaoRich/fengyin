#pragma once

#include <juce_core/juce_core.h>
#include <optional>
#include "MidiPerformanceSink.h"

namespace fengyin
{
struct SoundPreset
{
    juce::String id;
    juce::String name;
    juce::String pluginIdentifier;
    juce::MemoryBlock pluginState;
    juce::String effectIdentifier;
    juce::MemoryBlock effectState;
    bool effectBypassed = false;
    int breathController = 2;
    float breathCurve = 0.9f;
    float breathSmoothing = 0.28f;
    float eqTone = 0.2f;
    float warmth = 0.2f;
    float reverbMix = 0.28f;
    juce::String toneStyleId = "natural";
    juce::Array<TechniqueMapping> techniqueMappings;
    juce::String visualTheme = "neon";
    bool favorite = false; // Retained only for reading older preset files.
};

class SoundPresetStore final
{
public:
    explicit SoundPresetStore(juce::File storageDirectory = {});

    [[nodiscard]] juce::Array<SoundPreset> loadAll() const;
    [[nodiscard]] std::optional<SoundPreset> findById(const juce::String& id) const;
    bool save(const SoundPreset& preset);
    bool remove(const juce::String& id);
    bool setFavorite(const juce::String& id, bool favorite);
    bool setDefaultId(const juce::String& id);
    [[nodiscard]] juce::String getDefaultId() const;

private:
    [[nodiscard]] juce::File getFile() const;
    static std::unique_ptr<juce::XmlElement> toXml(const juce::Array<SoundPreset>& presets);
    static juce::Array<SoundPreset> fromXml(const juce::XmlElement& root);
    bool writeAll(const juce::Array<SoundPreset>& presets) const;

    juce::File directory;
};
}
