#include "SoundPresetStore.h"

namespace fengyin
{
SoundPresetStore::SoundPresetStore(juce::File storageDirectory)
{
    directory = storageDirectory == juce::File()
        ? juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory).getChildFile("FengYin")
        : std::move(storageDirectory);
}

juce::Array<SoundPreset> SoundPresetStore::loadAll() const
{
    if (auto xml = juce::XmlDocument::parse(getFile()); xml != nullptr
        && (xml->getIntAttribute("version") == 7 || xml->getIntAttribute("version") == 8))
        return fromXml(*xml);
    return {};
}

std::optional<SoundPreset> SoundPresetStore::findById(const juce::String& id) const
{
    for (const auto& preset : loadAll())
        if (preset.id == id)
            return preset;
    return std::nullopt;
}

bool SoundPresetStore::save(const SoundPreset& preset)
{
    if (preset.id.isEmpty() || preset.name.isEmpty() || preset.pluginIdentifier.isEmpty())
        return false;
    if (preset.samplerState.getSize() > 16 * 1024 * 1024) return false;

    auto presets = loadAll();
    bool replaced = false;
    for (auto& existing : presets)
    {
        if (existing.id == preset.id)
        {
            existing = preset;
            replaced = true;
            break;
        }
    }
    if (! replaced)
        presets.add(preset);

    return writeAll(presets);
}

bool SoundPresetStore::remove(const juce::String& id)
{
    auto presets = loadAll();
    for (int i = presets.size(); --i >= 0;)
        if (presets.getReference(i).id == id) presets.remove(i);
    if (getDefaultId() == id) setDefaultId({});
    return writeAll(presets);
}

bool SoundPresetStore::setDefaultId(const juce::String& id)
{
    directory.createDirectory();
    const auto target = directory.getChildFile("default-preset.txt");
    juce::TemporaryFile temporary(target);
    return temporary.getFile().replaceWithText(id) && temporary.overwriteTargetFileWithTemporary();
}

juce::String SoundPresetStore::getDefaultId() const
{
    return directory.getChildFile("default-preset.txt").loadFileAsString().trim();
}

bool SoundPresetStore::writeAll(const juce::Array<SoundPreset>& presets) const
{
    directory.createDirectory();
    juce::TemporaryFile temporary(getFile());
    const auto xml = toXml(presets);
    return xml != nullptr && xml->writeTo(temporary.getFile()) && temporary.overwriteTargetFileWithTemporary();
}

juce::File SoundPresetStore::getFile() const
{
    return directory.getChildFile("sound-presets.xml");
}

std::unique_ptr<juce::XmlElement> SoundPresetStore::toXml(const juce::Array<SoundPreset>& presets)
{
    auto root = std::make_unique<juce::XmlElement>("FENGYIN_SOUND_PRESETS");
    root->setAttribute("version", 8);
    for (const auto& preset : presets)
    {
        auto* child = root->createNewChildElement("PRESET");
        child->setAttribute("id", preset.id);
        child->setAttribute("name", preset.name);
        child->setAttribute("plugin", preset.pluginIdentifier);
        child->setAttribute("instrumentModelIndex", preset.instrumentModelIndex);
        child->setAttribute("eqTone", static_cast<double>(preset.eqTone));
        child->setAttribute("warmth", static_cast<double>(preset.warmth));
        child->setAttribute("reverbMix", static_cast<double>(preset.reverbMix));
        child->setAttribute("toneStyleId", preset.toneStyleId);
        child->setAttribute("baseToneStyleId", preset.baseToneStyleId);
        child->setAttribute("pluginBrand", preset.pluginBrand);
        child->setAttribute("instrumentKey", preset.instrumentKey);
        child->setAttribute("instrumentChineseName", preset.instrumentChineseName);
        child->setAttribute("pluginProgramName", preset.pluginProgramName);
        if (preset.pluginBrand == "kong" && preset.samplerState.getSize() > 0)
            child->createNewChildElement("SAMPLER_STATE")->addTextElement(preset.samplerState.toBase64Encoding());
        child->setAttribute("customTone", preset.customTone);
        child->setAttribute("compressionThreshold", static_cast<double>(preset.compressionThreshold));
        child->setAttribute("compressionRatio", static_cast<double>(preset.compressionRatio));
        child->setAttribute("saturation", static_cast<double>(preset.saturation));
        child->setAttribute("harshControl", static_cast<double>(preset.harshControl));
        child->setAttribute("reverbRoomSize", static_cast<double>(preset.reverbRoomSize));
        child->setAttribute("reverbDamping", static_cast<double>(preset.reverbDamping));
        child->setAttribute("reverbWidth", static_cast<double>(preset.reverbWidth));
        child->setAttribute("outputGain", static_cast<double>(preset.outputGain));
        child->setAttribute("bass", static_cast<double>(preset.bass));
        for (const auto& parameter : preset.toneParameters)
        {
            auto* stored = child->createNewChildElement("TONE_PARAMETER");
            stored->setAttribute("id", parameter.identifier);
            stored->setAttribute("value", static_cast<double>(parameter.value));
        }
    }
    return root;
}

juce::Array<SoundPreset> SoundPresetStore::fromXml(const juce::XmlElement& root)
{
    juce::Array<SoundPreset> presets;
    if (! root.hasTagName("FENGYIN_SOUND_PRESETS"))
        return presets;

    for (auto* child : root.getChildIterator())
    {
        if (! child->hasTagName("PRESET"))
            continue;
        SoundPreset preset;
        preset.id = child->getStringAttribute("id");
        preset.name = child->getStringAttribute("name");
        preset.pluginIdentifier = child->getStringAttribute("plugin");
        preset.instrumentModelIndex = child->getIntAttribute("instrumentModelIndex", -1);
        preset.eqTone = static_cast<float>(child->getDoubleAttribute("eqTone", 0.2));
        preset.warmth = static_cast<float>(child->getDoubleAttribute("warmth", 0.2));
        preset.reverbMix = static_cast<float>(child->getDoubleAttribute("reverbMix", 0.28));
        preset.toneStyleId = child->getStringAttribute("toneStyleId", "natural");
        preset.baseToneStyleId = child->getStringAttribute("baseToneStyleId", preset.toneStyleId);
        preset.pluginBrand = child->getStringAttribute("pluginBrand", "swam");
        preset.instrumentKey = child->getStringAttribute("instrumentKey");
        preset.instrumentChineseName = child->getStringAttribute("instrumentChineseName");
        preset.pluginProgramName = child->getStringAttribute("pluginProgramName");
        if (preset.pluginBrand == "kong")
            if (const auto* state = child->getChildByName("SAMPLER_STATE"))
            {
                const auto encoded = state->getAllSubText();
                if (encoded.length() <= 24 * 1024 * 1024 && ! preset.samplerState.fromBase64Encoding(encoded))
                    preset.samplerState.reset();
            }
        preset.customTone = child->getBoolAttribute("customTone", false);
        preset.compressionThreshold = static_cast<float>(child->getDoubleAttribute("compressionThreshold", 0.58));
        preset.compressionRatio = static_cast<float>(child->getDoubleAttribute("compressionRatio", 1.7));
        preset.saturation = static_cast<float>(child->getDoubleAttribute("saturation", 0.04));
        preset.harshControl = static_cast<float>(child->getDoubleAttribute("harshControl", 0.10));
        preset.reverbRoomSize = static_cast<float>(child->getDoubleAttribute("reverbRoomSize", 0.42));
        preset.reverbDamping = static_cast<float>(child->getDoubleAttribute("reverbDamping", 0.54));
        preset.reverbWidth = static_cast<float>(child->getDoubleAttribute("reverbWidth", 0.88));
        preset.outputGain = static_cast<float>(child->getDoubleAttribute("outputGain", 1.0));
        preset.bass = static_cast<float>(child->getDoubleAttribute("bass", 0.0));
        for (auto* stored : child->getChildIterator())
            if (stored->hasTagName("TONE_PARAMETER"))
            {
                ToneParameterValue parameter;
                parameter.identifier = stored->getStringAttribute("id");
                parameter.value = static_cast<float>(stored->getDoubleAttribute("value"));
                if (parameter.identifier.isNotEmpty()) preset.toneParameters.add(std::move(parameter));
            }
        if (preset.id.isNotEmpty() && preset.name.isNotEmpty() && preset.pluginIdentifier.isNotEmpty())
            presets.add(std::move(preset));
    }
    return presets;
}
}
