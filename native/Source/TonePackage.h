#pragma once
#include "SoundPresetStore.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_cryptography/juce_cryptography.h>

namespace fengyin
{
// Data-only package. Plugin installers, licences and sample libraries are never
// discovered/copied implicitly from a tuner's machine.
struct TonePackage
{
    static bool write(const SoundPreset& draft, const juce::File& target)
    {
        if (draft.name.isEmpty() || draft.instrumentChineseName.isEmpty()
            || (draft.pluginBrand == "kong" && draft.samplerState.getSize() == 0)) return false;
        auto preset = draft;
        preset.studioDraft = false;
        juce::Array<SoundPreset> presets;
        presets.add(preset);
        const auto xml = SoundPresetStore::toXml(presets)->toString();
        juce::ZipFile::Builder zip;
        juce::Array<juce::var> files, dependencies;
        const auto add = [&zip, &files](const juce::String& name, const void* bytes, size_t size)
        {
            zip.addEntry(std::make_unique<juce::MemoryInputStream>(bytes, size, true), 6, name, juce::Time::getCurrentTime());
            auto* item = new juce::DynamicObject();
            item->setProperty("path", name);
            item->setProperty("sha256", juce::SHA256(bytes, size).toHexString());
            item->setProperty("bytes", static_cast<juce::int64>(size));
            files.add(juce::var(item));
        };
        add("tone.xml", xml.toRawUTF8(), xml.getNumBytesAsUTF8());
        if (preset.containerProjectState.getSize() > 0)
            add("instrument.KAM", preset.containerProjectState.getData(), preset.containerProjectState.getSize());
        const auto dependency = [&dependencies](const juce::String& descriptionXml, const char* role, bool bypassed)
        {
            juce::PluginDescription description;
            auto node = juce::parseXML(descriptionXml);
            if (! node || ! description.loadFromXml(*node)) return;
            auto* item = new juce::DynamicObject();
            item->setProperty("role", role);
            item->setProperty("name", description.name);
            item->setProperty("manufacturer", description.manufacturerName);
            item->setProperty("version", description.version);
            item->setProperty("format", description.pluginFormatName);
            item->setProperty("uid", description.uniqueId);
            item->setProperty("bypassed", bypassed);
            dependencies.add(juce::var(item));
        };
        dependency(preset.instrumentDescriptionXml, "instrument", false);
        for (const auto& effect : preset.effects) dependency(effect.descriptionXml, "effect", effect.bypassed);
        auto* root = new juce::DynamicObject();
        juce::var manifest(root);
        root->setProperty("format", "fengyin-tone-package");
        root->setProperty("version", 1);
        root->setProperty("instrument", preset.instrumentChineseName);
        root->setProperty("style", preset.name);
        root->setProperty("dependencies", dependencies);
        root->setProperty("files", files);
        root->setProperty("pluginBinariesIncluded", false);
        const auto json = juce::JSON::toString(manifest);
        zip.addEntry(std::make_unique<juce::MemoryInputStream>(json.toRawUTF8(), json.getNumBytesAsUTF8(), true),
                     6, "manifest.json", juce::Time::getCurrentTime());
        juce::TemporaryFile temporary(target);
        auto output = temporary.getFile().createOutputStream();
        if (! output || ! zip.writeToStream(*output, nullptr)) return false;
        output->flush();
        if (output->getStatus().failed()) return false;
        output.reset();
        return temporary.overwriteTargetFileWithTemporary();
    }
};
}
