#pragma once

#include <juce_data_structures/juce_data_structures.h>
#include <optional>

namespace fengyin
{
// QinEngine .KAM projects are JUCE ValueTree streams. Keeping this adapter
// separate from generic VST state lets the host restore the sampler rack even
// when the plug-in's generic state omits the selected KAI slot.
class KongProjectFile final
{
public:
    struct Description
    {
        juce::String kai;
        juce::String preset;
        int midiChannel = 0;
        int audioOutput = 0;
    };

    static std::optional<Description> describe(const juce::MemoryBlock& data)
    {
        if (data.getSize() == 0) return std::nullopt;
        const auto root = juce::ValueTree::readFromData(data.getData(), data.getSize());
        if (! root.isValid() || ! root.hasType("KAMFileRoot")) return std::nullopt;
        const auto rack = root.getChildWithName("RackParam");
        const auto list = root.getChildWithName("PresetList");
        if (! rack.isValid() || ! list.isValid()) return std::nullopt;
        for (int index = 0; index < list.getNumChildren(); ++index)
        {
            const auto slot = list.getChild(index);
            if (! slot.hasType("Preset") || ! static_cast<bool>(slot.getProperty("IsSelected", false))) continue;
            Description result;
            result.kai = slot.getProperty("KAI").toString();
            result.preset = slot.getProperty("Preset", "Sus_Main").toString();
            result.midiChannel = static_cast<int>(slot.getProperty("MIDI", 0));
            result.audioOutput = static_cast<int>(slot.getProperty("Audio", 0));
            if (result.kai.isNotEmpty()) return result;
        }
        return std::nullopt;
    }

    static juce::MemoryBlock create(const juce::String& kai,
                                    const juce::String& preset = "Sus_Main")
    {
        if (kai.trim().isEmpty()) return {};
        juce::ValueTree root("KAMFileRoot");
        juce::ValueTree rack("RackParam");
        rack.setProperty("MainTune", 440.0, nullptr);
        rack.setProperty("Poly", 102, nullptr);
        rack.setProperty("TuningType", 301, nullptr);
        rack.setProperty("ImpulseType", 602, nullptr);
        rack.setProperty("Gain", 0.0, nullptr);
        rack.setProperty("PitchbendUp", 3, nullptr);
        rack.setProperty("PitchbendDown", 3, nullptr);
        rack.setProperty("IsOptimizationOn", false, nullptr);

        juce::ValueTree list("PresetList");
        juce::ValueTree slot("Preset");
        slot.setProperty("KAI", kai.trim(), nullptr);
        slot.setProperty("Preset", preset.isNotEmpty() ? preset : juce::String("Sus_Main"), nullptr);
        slot.setProperty("IsSelected", true, nullptr);
        slot.setProperty("MIDI", 0, nullptr);
        slot.setProperty("Audio", 0, nullptr);
        slot.setProperty("Volume", 127.0, nullptr);
        slot.setProperty("Tone", 40.0, nullptr);
        slot.setProperty("Balance", 0.0, nullptr);
        slot.setProperty("FXAUX", 20.0, nullptr);
        slot.setProperty("IndexSelectedKeyswitch", 0, nullptr);
        list.addChild(slot, -1, nullptr);
        root.addChild(rack, -1, nullptr);
        root.addChild(list, -1, nullptr);

        juce::MemoryOutputStream output;
        root.writeToStream(output);
        return output.getMemoryBlock();
    }
};
}
