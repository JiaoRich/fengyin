#pragma once
#include <juce_data_structures/juce_data_structures.h>
namespace fengyin {
struct KongTechniqueBinding { int channel = 0; int keyswitch = -1; int normalKey = -1; };
inline KongTechniqueBinding findKongTechniqueAt(const juce::ValueTree& tree,int channel,int index)
{
    if(channel<1||channel>16||index<0) return {};
    for(auto slot:tree.getChildWithName("PresetList")) {
        if(int(slot.getProperty("MIDI",0))+1!=channel) continue;
        const auto value=slot.getProperty("KeyswitchNames");const auto* names=value.getArray();
        const int low=slot.getProperty("KeyswitchLo",-1),high=slot.getProperty("KeyswitchHi",-1);
        const int selected=slot.getProperty("IndexSelectedKeyswitch",0);
        if(!names||index>=names->size()||selected<0||selected>=names->size()||low<0||high>127||low+index>high||low+selected>high) return {};
        // Independent presets have no actual keyswitch note; the destination
        // channel alone selects them. Do not fabricate a MIDI Note 0.
        if(low==0&&high==0&&names->size()==1) return {channel,-1,-1};
        return {channel,low+index,low+selected};
    }
    return {};
}
inline KongTechniqueBinding findKongTechnique(const juce::ValueTree& tree, const juce::StringArray& names)
{
    for (auto slot : tree.getChildWithName("PresetList"))
    {
        const auto value = slot.getProperty("KeyswitchNames");
        const auto* available = value.getArray();
        if (!available) continue;
        const int low = slot.getProperty("KeyswitchLo", -1);
        const int high = slot.getProperty("KeyswitchHi", -1);
        const int channel = static_cast<int>(slot.getProperty("MIDI", 0)) + 1;
        const int normal = low + static_cast<int>(slot.getProperty("IndexSelectedKeyswitch", 0));
        if (low < 0 || high > 127 || normal < low || normal > high || channel < 1 || channel > 16) continue;
        for (const auto& wanted : names)
            for (int i = 0; i < available->size() && low + i <= high; ++i)
                if ((*available)[i].toString() == wanted)
                    return {channel, low + i, normal};
    }
    return {};
}
}
