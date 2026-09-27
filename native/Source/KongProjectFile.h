#pragma once

#include <juce_data_structures/juce_data_structures.h>
#include <cstring>
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

    // QinEngine deliberately uses two layers:
    //   .KAM file: KAMFileRoot { RackParam, PresetList }
    //   JUCE VST3 state: VST3PluginState XML wrapping the component's
    //                    State { RackParam, PresetList } ValueTree.
    // Never persist a guessed KAM when the live plug-in can provide its exact
    // selected KAI and articulation. Convert the state captured from QinEngine
    // into the native project format instead.
    static juce::MemoryBlock fromPluginState(const juce::MemoryBlock& pluginState)
    {
        if (pluginState.getSize() == 0) return {};
        const auto state = readPluginComponentState(pluginState);
        if (! state.isValid()) return {};
        // Some QinEngine builds return their native project payload directly
        // from getStateInformation. Preserve it rather than rejecting a valid
        // KAM merely because the wrapper root differs between platforms.
        if (state.hasType("KAMFileRoot"))
            return hasSelectedPreset(state.getChildWithName("PresetList")) ? pluginState : juce::MemoryBlock();
        if (! state.hasType("State")) return {};
        const auto sourceList = state.getChildWithName("PresetList");
        if (! sourceList.isValid()) return {};
        for (int index = 0; index < sourceList.getNumChildren(); ++index)
        {
            const auto slot = sourceList.getChild(index);
            if (! slot.hasType("Preset")) continue;
            const auto kai = slot.getProperty("KAI").toString();
            const auto preset = slot.getProperty("Preset").toString();
            if (kai.isEmpty() || preset.isEmpty()) continue;

            // QinEngine's VST3 component state deliberately omits IsSelected
            // and contains runtime-only fields such as NoteRanges. A native
            // KAM contains the same exact KAI/preset identity in a compact,
            // canonical schema. Build that schema from the values selected in
            // the live editor; never infer them from the user's display name.
            return create(kai, preset);
        }
        return {};
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
        juce::ValueTree rack("RackParam");
        rack.setProperty("MainTune", 440.0, nullptr);
        rack.setProperty("Poly", 1.0, nullptr);
        rack.setProperty("TuningType", 0.0, nullptr);
        rack.setProperty("IsOptimizationOn", 0.0, nullptr);
        rack.setProperty("ImpulseType", 1.0, nullptr);
        rack.setProperty("Gain", 0.0, nullptr);
        rack.setProperty("Display", 1, nullptr);
        rack.setProperty("PitchbendUp", 10.0, nullptr);
        rack.setProperty("PitchbendDown", 2.0, nullptr);

        juce::ValueTree list("PresetList");
        for (int index = 0; index < sourceList.getNumChildren(); ++index)
        {
            const auto source = sourceList.getChild(index);
            if (! source.hasType("Preset")
                || source.getProperty("KAI").toString().isEmpty()
                || source.getProperty("Preset").toString().isEmpty()) continue;
            juce::ValueTree slot("Preset");
            for (const auto* property : { "KAI", "Preset", "MIDI", "Audio", "Volume", "Tone",
                                          "Balance", "FXAUX", "IndexSelectedKeyswitch" })
                if (source.hasProperty(property))
                    slot.setProperty(property, source.getProperty(property), nullptr);
            list.addChild(slot, -1, nullptr);
        }
        if (list.getNumChildren() == 0) return {};
        state.addChild(rack, -1, nullptr);
        state.addChild(list, -1, nullptr);
        return wrapVst3ComponentState(serialise(state));
    }

private:
    static juce::ValueTree readPluginComponentState(const juce::MemoryBlock& data)
    {
        // JUCE's VST3 host wraps the plug-in's IComponent state in a
        // VST3PluginState XML binary (magic "VC2!"). The previous code tried
        // to parse this wrapper as a ValueTree and therefore never reached the
        // exact KAI and articulation selected by the user.
        if (data.getSize() <= 8 || juce::ByteOrder::littleEndianInt(data.getData()) != 0x21324356u)
        {
            auto direct = juce::ValueTree::readFromData(data.getData(), data.getSize());
            return direct.hasType("State") || direct.hasType("KAMFileRoot") ? direct : juce::ValueTree();
        }
        const auto declaredLength = static_cast<int>(juce::ByteOrder::littleEndianInt(
            static_cast<const char*>(data.getData()) + 4));
        if (declaredLength <= 0 || declaredLength > static_cast<int>(data.getSize() - 8)) return {};
        auto wrapper = juce::parseXML(juce::String::fromUTF8(
            static_cast<const char*>(data.getData()) + 8, declaredLength));
        if (wrapper == nullptr || ! wrapper->hasTagName("VST3PluginState")) return {};
        const auto* component = wrapper->getChildByName("IComponent");
        if (component == nullptr) return {};
        juce::MemoryBlock componentState;
        if (! componentState.fromBase64Encoding(component->getAllSubText())) return {};
        return juce::ValueTree::readFromData(componentState.getData(), componentState.getSize());
    }

    static juce::MemoryBlock wrapVst3ComponentState(const juce::MemoryBlock& componentState)
    {
        if (componentState.getSize() == 0) return {};
        juce::XmlElement wrapper("VST3PluginState");
        wrapper.createNewChildElement("IComponent")->addTextElement(componentState.toBase64Encoding());
        juce::MemoryBlock result;
        {
            juce::MemoryOutputStream output(result, false);
            output.writeInt(static_cast<int>(0x21324356u));
            output.writeInt(0);
            wrapper.writeTo(output, juce::XmlElement::TextFormat().singleLine());
            output.writeByte(0);
        }
        const auto textLength = juce::ByteOrder::swapIfBigEndian(static_cast<juce::uint32>(result.getSize() - 9));
        std::memcpy(static_cast<char*>(result.getData()) + 4, &textLength, sizeof(textLength));
        return result;
    }

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
