#include "SwamExpressionCurve.h"

namespace fengyin
{
namespace
{
SwamExpressionCurve::Result findAndApply(juce::XmlElement& element)
{
    if (element.hasTagName("MIDIRemappingEntry")
        && element.getStringAttribute("parameterId").equalsIgnoreCase("expression"))
    {
        SwamExpressionCurve::Result result;
        result.found = true;
        result.controller = element.getIntAttribute("msb", -1);
        if (auto* curve = element.getChildByName("MIDIRemappingCurve"))
        {
            const auto set = [&result, curve](const char* name, double value)
            {
                if (! juce::approximatelyEqual(curve->getDoubleAttribute(name), value))
                {
                    curve->setAttribute(name, value);
                    result.changed = true;
                }
            };
            set("input_min", SwamExpressionCurve::inputMinimum);
            set("input_max", SwamExpressionCurve::inputMaximum);
            set("out_min", SwamExpressionCurve::outputMinimum);
            set("out_max", SwamExpressionCurve::outputMaximum);
            set("shape", SwamExpressionCurve::shape);
            set("symmetry", SwamExpressionCurve::symmetry);
            if (curve->getIntAttribute("bypass", 0) != 0)
            {
                curve->setAttribute("bypass", 0);
                result.changed = true;
            }
            if (curve->getIntAttribute("bipolar", 0) != 0)
            {
                curve->setAttribute("bipolar", 0);
                result.changed = true;
            }
        }
        return result;
    }

    for (auto* child = element.getFirstChildElement(); child != nullptr; child = child->getNextElement())
    {
        auto result = findAndApply(*child);
        if (result.found) return result;
    }
    return {};
}
}

SwamExpressionCurve::Result SwamExpressionCurve::applyToXml(juce::XmlElement& root)
{
    return findAndApply(root);
}

SwamExpressionCurve::Result SwamExpressionCurve::applyToState(juce::MemoryBlock& state)
{
    if (state.isEmpty()) return {};

    bool binaryXml = false;
    auto xml = juce::AudioProcessor::getXmlFromBinary(state.getData(), static_cast<int>(state.getSize()));
    if (xml != nullptr)
    {
        binaryXml = true;
    }
    else
    {
        const auto text = juce::String::fromUTF8(static_cast<const char*>(state.getData()),
                                                 static_cast<int>(state.getSize()));
        xml = juce::parseXML(text);
    }
    if (xml == nullptr) return {};

    auto result = applyToXml(*xml);
    if (! result.found || ! result.changed) return result;

    if (binaryXml)
    {
        juce::AudioProcessor::copyXmlToBinary(*xml, state);
    }
    else
    {
        const auto text = xml->toString();
        state.replaceAll(text.toRawUTF8(), static_cast<size_t>(text.getNumBytesAsUTF8()));
    }
    return result;
}
}
