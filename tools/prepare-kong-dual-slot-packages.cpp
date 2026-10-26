#include "TonePackage.h"
#include "KongProjectFile.h"
#include <CoreFoundation/CoreFoundation.h>
#include <iostream>

int main(int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    if (argc != 4) return 2;
    const juce::File source(juce::String::fromUTF8(argv[1]));
    const juce::File output(juce::String::fromUTF8(argv[2]));
    if (!output.createDirectory()) return 3;
    juce::AudioPluginFormatManager formats;
    formats.addFormat(std::make_unique<juce::VST3PluginFormat>());
    juce::OwnedArray<juce::PluginDescription> types;
    formats.getFormat(0)->findAllTypesForFile(types, juce::String::fromUTF8(argv[3]));
    if (types.isEmpty()) return 4;
    struct Target { const char* file; const char* articulation; };
    for (const auto target : {Target{"folk-pipa.fytonepack", "Roll_3"},
                              Target{"folk-liuqin.fytonepack", "Roll_2"},
                              Target{"folk-jinghu.fytonepack", "Tremolo"}}) {
        juce::String error;
        auto preset = fengyin::TonePackage::read(source.getChildFile(target.file), error);
        if (!preset) { std::cerr << error << '\n'; return 5; }
        auto tree = fengyin::KongProjectFile::readPluginComponentState(preset->samplerState);
        const auto before = tree.createCopy();
        auto slots = tree.getChildWithName("PresetList");
        if (slots.getNumChildren() != 1 || int(slots.getChild(0)["MIDI"]) != 0
            || preset->kongTechniques.otherChannel != 1) return 6;
        const auto first = slots.getChild(0);
        const auto names = first["KeyswitchNames"];
        const auto techniqueIndex = preset->kongTechniques.otherKeyswitchIndex;
        if (!names.isArray() || techniqueIndex < 0 || techniqueIndex >= names.size()
            || names[techniqueIndex].toString() != target.articulation) return 7;
        auto second = first.createCopy();
        second.setProperty("MIDI", 1, nullptr);
        second.setProperty("IndexSelectedKeyswitch", techniqueIndex, nullptr);
        slots.addChild(second, -1, nullptr);
        juce::MemoryOutputStream stream;
        tree.writeToStream(stream);
        auto wrapper = juce::AudioProcessor::getXmlFromBinary(preset->samplerState.getData(),
            static_cast<int>(preset->samplerState.getSize()));
        if (!wrapper || !wrapper->getChildByName("IComponent")) return 8;
        auto* component = wrapper->getChildByName("IComponent");
        component->deleteAllTextElements();
        component->addTextElement(stream.getMemoryBlock().toBase64Encoding());
        juce::MemoryBlock candidate;
        juce::AudioProcessor::copyXmlToBinary(*wrapper, candidate);
        auto plugin = formats.createPluginInstance(*types[0], 44100, 128, error);
        if (!plugin) return 9;
        CFRunLoopRunInMode(kCFRunLoopDefaultMode, .5, false);
        plugin->setStateInformation(candidate.getData(), static_cast<int>(candidate.getSize()));
        CFRunLoopRunInMode(kCFRunLoopDefaultMode, .5, false);
        plugin->getStateInformation(preset->samplerState);
        const auto actual = fengyin::KongProjectFile::readPluginComponentState(preset->samplerState);
        const auto actualSlots = actual.getChildWithName("PresetList");
        if (actualSlots.getNumChildren() != 2
            || !actualSlots.getChild(0).isEquivalentTo(before.getChildWithName("PresetList").getChild(0))
            || !actual.getChildWithName("RackParam").isEquivalentTo(before.getChildWithName("RackParam"))) return 10;
        const auto loaded = actualSlots.getChild(1);
        const auto actualNames = loaded["KeyswitchNames"];
        if (int(loaded["MIDI"]) != 1 || loaded["Preset"] != first["Preset"]
            || int(loaded["IndexSelectedKeyswitch"]) != techniqueIndex
            || actualNames[techniqueIndex].toString() != target.articulation) return 11;
        for (const auto* field : {"KAI", "Audio", "Volume", "Tone", "Balance", "FXAUX"})
            if (loaded[field] != first[field]) return 12;
        preset->containerProjectState = fengyin::KongProjectFile::fromPluginState(
            preset->samplerState, preset->containerProjectState);
        preset->kongTechniques.otherChannel = 2;
        preset->kongTechniques.otherKeyswitchIndex = -1;
        if (preset->containerProjectState.isEmpty()
            || !fengyin::TonePackage::write(*preset, output.getChildFile(target.file))) return 13;
        const auto returned = fengyin::TonePackage::read(output.getChildFile(target.file), error);
        if (!returned || returned->samplerState != preset->samplerState
            || returned->containerProjectState != preset->containerProjectState
            || returned->kongTechniques.otherChannel != 2
            || returned->kongTechniques.otherKeyswitchIndex != -1) return 14;
        std::cout << target.file << ": original CH1/rack preserved, CH2=" << target.articulation
                  << ", native restore and package roundtrip passed\n";
    }
}
