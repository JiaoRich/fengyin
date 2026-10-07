#pragma once
#include <juce_data_structures/juce_data_structures.h>

namespace fengyin
{
// Unmodified states captured from QinEngine; KAM lacks runtime note maps.
struct KongReleaseStates
{
    static juce::MemoryBlock read(const juce::String& xml, const juce::String& asset)
    {
        auto root = juce::parseXML(xml);
        if (!root || !root->hasTagName("QIN_RELEASE_STATES")) return {};
        for (auto* entry : root->getChildIterator())
        {
            if (entry->getStringAttribute("asset") != asset) continue;
            juce::MemoryBlock state;
            if (!state.fromBase64Encoding(entry->getAllSubText()) || state.getSize() < 9) return {};
            const auto* bytes = static_cast<const char*>(state.getData());
            if (juce::ByteOrder::littleEndianInt(bytes) != 0x21324356) return {};
            const auto length = juce::ByteOrder::littleEndianInt(bytes + 4);
            if (length == 0 || length > state.getSize() - 8) return {};
            auto wrapper = juce::parseXML(juce::String::fromUTF8(bytes + 8, static_cast<int>(length)));
            if (!wrapper || !wrapper->hasTagName("VST3PluginState")) return {};
            auto* component = wrapper->getChildByName("IComponent");
            juce::MemoryBlock payload;
            if (!component || !payload.fromBase64Encoding(component->getAllSubText())) return {};
            auto tree = juce::ValueTree::readFromData(payload.getData(), payload.getSize());
            auto slots = tree.getChildWithName("PresetList");
            if (!tree.hasType("State") || !tree.getChildWithName("RackParam").isValid()
                || slots.getNumChildren() == 0) return {};
            for (auto slot : slots)
                if (slot.getProperty("KAI").toString().isEmpty()
                    || !slot.hasProperty("NoteRanges")) return {};
            return state;
        }
        return {};
    }
};
}
