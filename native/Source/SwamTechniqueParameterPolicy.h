#pragma once
#include <juce_core/juce_core.h>
namespace fengyin {
struct SwamTechniqueParameterPolicy {
    static bool isPortamentoTime(const juce::String& displayName) {
        const auto name=displayName.toLowerCase().removeCharacters(" ._-\r\n");
        return name=="portamentotime"; // Never Control, Max Time, Split Ratio, etc.
    }
    static bool hasNativePortamentoInput(const juce::XmlElement& node) {
        if(node.hasTagName("MIDIRemappingEntry") && isPortamentoTime(node.getStringAttribute("parameterId"))
            && node.getIntAttribute("enabled")==1 && node.getIntAttribute("messageType",-1)!=1) return true;
        for(auto* child:node.getChildIterator())if(hasNativePortamentoInput(*child))return true;
        return false;
    }
};
}
