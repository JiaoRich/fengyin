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

    // QinEngine deliberately uses two closely-related ValueTree schemas:
    //   .KAM file: KAMFileRoot { RackParam(TuningType), PresetList }
    //   VST state: State { Zoom, RackParam(Tuning), PresetList }
    // Never persist a guessed KAM when the live plug-in can provide its exact
    // selected KAI and articulation. Convert the state captured from QinEngine
    // into the native project format instead.
    static juce::MemoryBlock fromPluginState(const juce::MemoryBlock& pluginState)
    {
        if (pluginState.getSize() == 0) return {};
        const auto state = juce::ValueTree::readFromData(pluginState.getData(), pluginState.getSize());
        if (! state.isValid() || ! state.hasType("State")) return {};
        const auto sourceRack = state.getChildWithName("RackParam");
        const auto sourceList = state.getChildWithName("PresetList");
        if (! sourceRack.isValid() || ! sourceList.isValid() || ! hasSelectedPreset(sourceList)) return {};

        auto rack = sourceRack.createCopy();
        if (rack.hasProperty("Tuning"))
        {
            rack.setProperty("TuningType", rack.getProperty("Tuning"), nullptr);
            rack.removeProperty("Tuning", nullptr);
        }
        juce::ValueTree root("KAMFileRoot");
        root.addChild(rack, -1, nullptr);
        root.addChild(sourceList.createCopy(), -1, nullptr);
        return serialise(root);
    }

    // Convert a native .KAM project into the exact schema consumed by
    // AudioProcessor::setStateInformation. This invokes QinEngine's own
    // clearMulti/addKAI/addPreset restore path, just as a DAW project restore
    // does, without simulating editor clicks or rewriting parameters later.
    static juce::MemoryBlock toPluginState(const juce::MemoryBlock& project)
    {
        if (project.getSize() == 0) return {};
        const auto kam = juce::ValueTree::readFromData(project.getData(), project.getSize());
        if (! kam.isValid() || ! kam.hasType("KAMFileRoot")) return {};
        const auto sourceRack = kam.getChildWithName("RackParam");
        const auto sourceList = kam.getChildWithName("PresetList");
        if (! sourceRack.isValid() || ! sourceList.isValid() || ! hasSelectedPreset(sourceList)) return {};

        juce::ValueTree state("State");
        juce::ValueTree zoom("Zoom");
        zoom.setProperty("UI_Width", 1200, nullptr);
        zoom.setProperty("UI_Height", 760, nullptr);
        auto rack = sourceRack.createCopy();
        if (rack.hasProperty("TuningType"))
        {
            rack.setProperty("Tuning", rack.getProperty("TuningType"), nullptr);
            rack.removeProperty("TuningType", nullptr);
        }
        state.addChild(zoom, -1, nullptr);
        state.addChild(rack, -1, nullptr);
        state.addChild(sourceList.createCopy(), -1, nullptr);
        return serialise(state);
    }

private:
    static bool hasSelectedPreset(const juce::ValueTree& list)
    {
        for (int index = 0; index < list.getNumChildren(); ++index)
        {
            const auto slot = list.getChild(index);
            if (slot.hasType("Preset")
                && static_cast<bool>(slot.getProperty("IsSelected", false))
                && slot.getProperty("KAI").toString().isNotEmpty()
                && slot.getProperty("Preset").toString().isNotEmpty())
                return true;
        }
        return false;
    }

    static juce::MemoryBlock serialise(const juce::ValueTree& tree)
    {
        juce::MemoryOutputStream output;
        tree.writeToStream(output);
        return output.getMemoryBlock();
    }
};
}
