#pragma once

#include <juce_core/juce_core.h>
#include <optional>
#include "ToneParameterValue.h"

namespace fengyin
{
struct SoundPreset
{
    juce::String id;
    juce::String name;
    juce::String pluginIdentifier;
    juce::Array<ToneParameterValue> toneParameters;
    int instrumentModelIndex = -1;
    float eqTone = 0.2f;
    float warmth = 0.2f;
    float reverbMix = 0.28f;
    juce::String toneStyleId = "natural";
    juce::String baseToneStyleId = "natural";
    juce::String pluginBrand = "swam";
    juce::String instrumentKey;
    juce::String instrumentChineseName;
    juce::String pluginProgramName;
    // Full state for a user-created instrument inside a container plug-in.  Qin is
    // the first adapter; Kontakt/Falcon and other containers use the same field.
    juce::MemoryBlock samplerState;
    bool containerInstrument = false;
    juce::String containerAdapter;
    bool customTone = false;
    float compressionThreshold = 0.58f;
    float compressionRatio = 1.7f;
    float saturation = 0.04f;
    float harshControl = 0.10f;
    float reverbRoomSize = 0.42f;
    float reverbDamping = 0.54f;
    float reverbWidth = 0.88f;
    float outputGain = 1.0f;
    float bass = 0.0f;
};

class SoundPresetStore final
{
public:
    explicit SoundPresetStore(juce::File storageDirectory = {});

    [[nodiscard]] juce::Array<SoundPreset> loadAll() const;
    [[nodiscard]] std::optional<SoundPreset> findById(const juce::String& id) const;
    bool save(const SoundPreset& preset);
    bool remove(const juce::String& id);
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
