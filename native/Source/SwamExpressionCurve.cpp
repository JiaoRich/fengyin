#include "SwamExpressionCurve.h"

namespace fengyin
{
namespace
{
void inspect(juce::XmlElement& element, SwamExpressionCurve::Result& result, bool editable)
{
    if (element.hasTagName("MIDIRemappingEntry"))
    {
        const auto parameter = element.getStringAttribute("parameterId");
        const int cc = element.getIntAttribute("msb", -1);
        const int channel = element.getIntAttribute("channel", 17);
        if (element.getIntAttribute("enabled", 0) == 1
            && element.getIntAttribute("messageType", -1) == 1
            && element.getIntAttribute("lsb", -1) <= 0 && cc >= 0 && cc <= 127
            && channel >= 1 && channel <= 17)
        {
            result.routes.push_back({parameter, cc, channel == 17 ? 1 : channel});
            if (parameter.equalsIgnoreCase("expression"))
            {
                result.found = true;
                result.controller = cc;
                if (editable && element.getChildByName("MIDIRemappingCurve") == nullptr)
                {
                    auto* curve = element.createNewChildElement("MIDIRemappingCurve");
                    curve->setAttribute("input_min", 0); curve->setAttribute("input_max", 127);
                    curve->setAttribute("out_min", 0); curve->setAttribute("out_max", 127);
                    curve->setAttribute("shape", 0.0); curve->setAttribute("symmetry", 0.5);
                    curve->setAttribute("bypass", 0); curve->setAttribute("bipolar", 0);
                    result.changed = true;
                }
            }
        }
    }
    for (auto* child = element.getFirstChildElement(); child; child = child->getNextElement())
        inspect(*child, result, editable);
}

// Vendor binary envelopes are read-only. Never guess their size/checksum fields.
void inspectComponent(const juce::MemoryBlock& block, SwamExpressionCurve::Result& result)
{
    const std::string bytes(static_cast<const char*>(block.getData()), block.getSize());
    const auto begin = bytes.find("<MIDIRemappingTable");
    const auto end = bytes.find("</MIDIRemappingTable>", begin);
    if (begin == std::string::npos || end == std::string::npos) return;
    const auto text = bytes.substr(begin, end + std::string("</MIDIRemappingTable>").size() - begin);
    if (auto table = juce::parseXML(juce::String::fromUTF8(text.data(), static_cast<int>(text.size()))))
        inspect(*table, result, false);
}
}

SwamExpressionCurve::Result SwamExpressionCurve::applyToXml(juce::XmlElement& root)
{
    Result result;
    inspect(root, result, true);
    return result;
}

SwamExpressionCurve::Result SwamExpressionCurve::applyToState(juce::MemoryBlock& state)
{
    if (state.isEmpty()) return {};
    auto xml = juce::AudioProcessor::getXmlFromBinary(state.getData(), static_cast<int>(state.getSize()));
    const bool binary = xml != nullptr;
    if (!xml) xml = juce::parseXML(juce::String::fromUTF8(static_cast<const char*>(state.getData()), static_cast<int>(state.getSize())));
    Result result;
    if (!xml) { inspectComponent(state, result); return result; }
    if (xml->hasTagName("VST3PluginState"))
    {
        if (auto* component = xml->getChildByName("IComponent"))
        {
            const auto encoded = component->getAllSubText().trim();
            const int bytes = encoded.upToFirstOccurrenceOf(".", false, false).getIntValue();
            if (bytes > 0 && bytes <= 32 * 1024 * 1024)
            {
                juce::MemoryBlock payload;
                if (payload.fromBase64Encoding(encoded)) inspectComponent(payload, result);
            }
        }
        return result;
    }
    result = applyToXml(*xml);
    if (result.changed)
    {
        if (binary) juce::AudioProcessor::copyXmlToBinary(*xml, state);
        else { const auto text = xml->toString(); state.replaceAll(text.toRawUTF8(), static_cast<size_t>(text.getNumBytesAsUTF8())); }
    }
    return result;
}
}
