#pragma once
#include "SoundPresetStore.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_cryptography/juce_cryptography.h>
#include <cmath>

namespace fengyin
{
// Data-only package. Plugin installers, licences and sample libraries are never
// discovered/copied implicitly from a tuner's machine.
struct TonePackage
{
    static std::optional<SoundPreset> read(const juce::File& file, juce::String& error)
    {
        const auto fail = [&error](const char* message) -> std::optional<SoundPreset>
        { error = juce::String::fromUTF8(message); return std::nullopt; };
        constexpr juce::int64 limit = 256ll * 1024 * 1024;
        if (! file.existsAsFile() || file.getSize() <= 0 || file.getSize() > limit)
            return fail("方案包无法读取或超过 256 MB");
        juce::ZipFile zip(file);
        if (zip.getNumEntries() < 2 || zip.getNumEntries() > 3) return fail("不是完整的风吟方案包");
        juce::StringArray names;
        juce::int64 total = 0;
        for (int i = 0; i < zip.getNumEntries(); ++i)
        {
            const auto* entry = zip.getEntry(i);
            if (! entry || names.contains(entry->filename)
                || (entry->filename != "manifest.json" && entry->filename != "tone.xml" && entry->filename != "instrument.KAM"))
                return fail("方案包包含重复或不支持的文件");
            const auto entryLimit = entry->filename == "tone.xml" ? limit : 1024ll * 1024;
            if (entry->uncompressedSize < 0 || entry->uncompressedSize > entryLimit
                || (total += entry->uncompressedSize) > limit) return fail("方案包解压后过大");
            names.add(entry->filename);
        }
        // Read only into bounded memory; never extract paths or execute files.
        const auto readEntry = [&zip](const juce::String& name, juce::MemoryBlock& bytes)
        {
            const auto index = zip.getIndexOfFileName(name);
            if (index < 0) return false;
            const auto size = zip.getEntry(index)->uncompressedSize;
            std::unique_ptr<juce::InputStream> stream(zip.createStreamForEntry(index));
            if (! stream) return false;
            bytes.setSize(static_cast<size_t>(size));
            if (stream->read(bytes.getData(), static_cast<int>(size)) != size) return false;
            char extra;
            return stream->read(&extra, 1) == 0;
        };
        juce::MemoryBlock manifestBytes, toneBytes, kamBytes;
        if (! readEntry("manifest.json", manifestBytes) || ! readEntry("tone.xml", toneBytes))
            return fail("方案包缺少清单或音色数据");
        const auto manifest = juce::JSON::parse(manifestBytes.toString());
        if (manifest["format"].toString() != "fengyin-tone-package" || static_cast<int>(manifest["version"]) != 1
            || static_cast<bool>(manifest["pluginBinariesIncluded"])) return fail("方案包版本或格式不支持");
        const auto* files = manifest["files"].getArray();
        if (! files || files->size() != zip.getNumEntries() - 1) return fail("方案包清单不完整");
        juce::StringArray checked;
        for (const auto& item : *files)
        {
            const auto path = item["path"].toString();
            if (checked.contains(path) || (path != "tone.xml" && path != "instrument.KAM")) return fail("方案包清单异常");
            const auto* bytes = &toneBytes;
            if (path == "instrument.KAM")
            {
                if (! readEntry(path, kamBytes)) return fail("无法读取空音 KAM");
                bytes = &kamBytes;
            }
            if (static_cast<juce::int64>(item["bytes"]) != static_cast<juce::int64>(bytes->getSize())
                || item["sha256"].toString() != juce::SHA256(bytes->getData(), bytes->getSize()).toHexString())
                return fail("方案包校验失败，文件可能损坏，请重新导出");
            checked.add(path);
        }
        if (! checked.contains("tone.xml")) return fail("方案包缺少音色校验信息");
        if (toneBytes.toString().containsIgnoreCase("<!DOCTYPE")) return fail("音色数据含不支持的 XML 声明");
        auto xml = juce::parseXML(toneBytes.toString());
        if (! xml || xml->getIntAttribute("version") != 9) return fail("音色数据格式不支持");
        if (xml->getNumChildElements() != 1) return fail("方案包必须包含一套完整音色");
        const auto presets = SoundPresetStore::fromXml(*xml);
        if (presets.size() != 1 || presets[0].instrumentChineseName.isEmpty()) return fail("音色数据不完整");
        auto preset = presets[0];
        const float scalars[] { preset.eqTone, preset.bass, preset.air, preset.warmth, preset.reverbMix,
            preset.compressionThreshold, preset.compressionRatio, preset.saturation, preset.harshControl,
            preset.reverbRoomSize, preset.reverbDamping, preset.reverbWidth, preset.outputGain };
        for (const auto value : scalars)
            if (! std::isfinite(value) || std::abs(value) > 100) return fail("方案包包含异常效果参数");
        if (preset.outputGain < 0 || preset.outputGain > 2) return fail("方案包输出音量超出支持范围");
        for (const auto& parameter : preset.toneParameters)
            if (! std::isfinite(parameter.value) || parameter.value < 0 || parameter.value > 1)
                return fail("方案包包含异常音源参数");
        juce::PluginDescription description;
        auto instrumentXml = juce::parseXML(preset.instrumentDescriptionXml);
        if (! instrumentXml || ! description.loadFromXml(*instrumentXml) || description.pluginFormatName != "VST3")
            return fail("缺少可识别的 VST3 音源信息");
        for (const auto& effect : preset.effects)
        {
            auto effectXml = juce::parseXML(effect.descriptionXml);
            if (! effectXml || ! description.loadFromXml(*effectXml) || description.pluginFormatName != "VST3")
                return fail("效果器依赖信息不完整");
        }
        if (preset.pluginBrand == "kong" || preset.containerInstrument)
        {
            if (preset.samplerState.getSize() == 0 || kamBytes.getSize() == 0
                || kamBytes != preset.containerProjectState) return fail("空音状态或 KAM 不完整、不一致");
        }
        else if (preset.instrumentState.getSize() == 0 && preset.toneParameters.isEmpty())
            return fail("方案包缺少音源参数");
        preset.id = juce::Uuid().toString();
        preset.studioDraft = true;
        preset.published = false;
        preset.customTone = true;
        error.clear();
        return preset;
    }

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
