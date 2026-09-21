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
    if (auto xml = juce::XmlDocument::parse(getFile()))
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

bool SoundPresetStore::setFavorite(const juce::String& id, bool favorite)
{
    auto presets = loadAll();
    bool found = false;
    for (auto& preset : presets)
        if (preset.id == id) { preset.favorite = favorite; found = true; }
    return found && writeAll(presets);
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
    root->setAttribute("version", 5);
    for (const auto& preset : presets)
    {
        auto* child = root->createNewChildElement("PRESET");
        child->setAttribute("id", preset.id);
        child->setAttribute("name", preset.name);
        child->setAttribute("plugin", preset.pluginIdentifier);
        child->setAttribute("effect", preset.effectIdentifier);
        child->setAttribute("effectBypassed", preset.effectBypassed);
        child->setAttribute("breathController", preset.breathController);
        child->setAttribute("breathCurve", static_cast<double>(preset.breathCurve));
        child->setAttribute("breathSmoothing", static_cast<double>(preset.breathSmoothing));
        child->setAttribute("eqTone", static_cast<double>(preset.eqTone));
        child->setAttribute("warmth", static_cast<double>(preset.warmth));
        child->setAttribute("reverbMix", static_cast<double>(preset.reverbMix));
        child->setAttribute("toneStyleId", preset.toneStyleId);
        child->setAttribute("visualTheme", preset.visualTheme);
        child->setAttribute("favorite", preset.favorite);
        child->createNewChildElement("PLUGIN_STATE")->addTextElement(preset.pluginState.toBase64Encoding());
        child->createNewChildElement("EFFECT_STATE")->addTextElement(preset.effectState.toBase64Encoding());
        for (const auto& mapping : preset.techniqueMappings)
        {
            auto* mapped = child->createNewChildElement("TECHNIQUE");
            mapped->setAttribute("target", static_cast<int>(mapping.technique));
            mapped->setAttribute("sourceType", static_cast<int>(mapping.sourceType));
            mapped->setAttribute("sourceNumber", mapping.sourceNumber);
            mapped->setAttribute("toggle", mapping.toggle);
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
        preset.effectIdentifier = child->getStringAttribute("effect");
        preset.effectBypassed = child->getBoolAttribute("effectBypassed", false);
        preset.breathController = child->getIntAttribute("breathController", 2);
        preset.breathCurve = static_cast<float>(child->getDoubleAttribute("breathCurve", 0.9));
        preset.breathSmoothing = static_cast<float>(child->getDoubleAttribute("breathSmoothing", 0.28));
        preset.eqTone = static_cast<float>(child->getDoubleAttribute("eqTone", 0.2));
        preset.warmth = static_cast<float>(child->getDoubleAttribute("warmth", 0.2));
        preset.reverbMix = static_cast<float>(child->getDoubleAttribute("reverbMix", 0.28));
        preset.toneStyleId = child->getStringAttribute("toneStyleId", "natural");
        preset.visualTheme = child->getStringAttribute("visualTheme", "neon");
        preset.favorite = child->getBoolAttribute("favorite", false);
        if (auto* pluginState = child->getChildByName("PLUGIN_STATE"))
            preset.pluginState.fromBase64Encoding(pluginState->getAllSubText());
        else
            preset.pluginState.fromBase64Encoding(child->getAllSubText());
        if (auto* effectState = child->getChildByName("EFFECT_STATE"))
            preset.effectState.fromBase64Encoding(effectState->getAllSubText());
        for (auto* mapped : child->getChildIterator())
            if (mapped->hasTagName("TECHNIQUE"))
            {
                TechniqueMapping mapping;
                mapping.technique = static_cast<PerformanceTechnique>(juce::jlimit(0, static_cast<int>(PerformanceTechnique::count) - 1,
                    mapped->getIntAttribute("target", 0)));
                mapping.sourceType = static_cast<TechniqueSourceType>(juce::jlimit(0, static_cast<int>(TechniqueSourceType::note),
                    mapped->getIntAttribute("sourceType", 0)));
                mapping.sourceNumber = mapped->getIntAttribute("sourceNumber", -1);
                mapping.toggle = mapped->getBoolAttribute("toggle", false);
                if (mapping.sourceType != TechniqueSourceType::none)
                    preset.techniqueMappings.add(mapping);
            }
        if (preset.id.isNotEmpty() && preset.name.isNotEmpty() && preset.pluginIdentifier.isNotEmpty())
            presets.add(std::move(preset));
    }
    return presets;
}
}
