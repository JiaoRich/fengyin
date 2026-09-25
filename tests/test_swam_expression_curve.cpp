#include "SwamExpressionCurve.h"

#include <cassert>
#include <cmath>

namespace
{
bool near(double left, double right)
{
    return std::abs(left - right) < 0.000001;
}
}

int main()
{
    const auto source = juce::String(R"xml(<?xml version="1.0"?>
<swam><program><midimapping><MIDIRemappingTable>
  <MIDIRemappingEntry enabled="1" parameterId="expression" channel="17" messageType="1" msb="2" lsb="-1">
    <MIDIRemappingCurve input_min="4" input_max="120" out_min="9" out_max="127" shape="-0.4" symmetry="0.7" bypass="1" bipolar="1"/>
  </MIDIRemappingEntry>
  <MIDIRemappingEntry enabled="1" parameterId="growl" channel="17" messageType="1" msb="74" lsb="-1">
    <MIDIRemappingCurve input_min="12" input_max="127" out_min="0" out_max="80" shape="0.3" symmetry="0.2" bypass="0" bipolar="0"/>
  </MIDIRemappingEntry>
</MIDIRemappingTable></midimapping></program></swam>)xml");
    auto xml = juce::parseXML(source);
    assert(xml != nullptr);

    const auto result = fengyin::SwamExpressionCurve::applyToXml(*xml);
    assert(result.found);
    assert(result.changed);
    assert(result.controller == 2);

    auto* table = xml->getChildByName("program")->getChildByName("midimapping")
        ->getChildByName("MIDIRemappingTable");
    auto* expression = table->getFirstChildElement();
    auto* curve = expression->getChildByName("MIDIRemappingCurve");
    assert(expression->getIntAttribute("msb") == 2);
    assert(expression->getIntAttribute("channel") == 17);
    assert(near(curve->getDoubleAttribute("input_min"), fengyin::SwamExpressionCurve::inputMinimum));
    assert(near(curve->getDoubleAttribute("input_max"), fengyin::SwamExpressionCurve::inputMaximum));
    assert(near(curve->getDoubleAttribute("out_min"), fengyin::SwamExpressionCurve::outputMinimum));
    assert(near(curve->getDoubleAttribute("out_max"), fengyin::SwamExpressionCurve::outputMaximum));
    assert(near(curve->getDoubleAttribute("shape"), fengyin::SwamExpressionCurve::shape));
    assert(near(curve->getDoubleAttribute("symmetry"), fengyin::SwamExpressionCurve::symmetry));
    assert(curve->getIntAttribute("bypass") == 0);
    assert(curve->getIntAttribute("bipolar") == 0);

    auto* growl = expression->getNextElement();
    auto* growlCurve = growl->getChildByName("MIDIRemappingCurve");
    assert(growl->getIntAttribute("msb") == 74);
    assert(near(growlCurve->getDoubleAttribute("input_min"), 12.0));
    assert(near(growlCurve->getDoubleAttribute("out_max"), 80.0));

    juce::MemoryBlock plain(source.toRawUTF8(), static_cast<size_t>(source.getNumBytesAsUTF8()));
    const auto plainResult = fengyin::SwamExpressionCurve::applyToState(plain);
    assert(plainResult.found && plainResult.changed && plainResult.controller == 2);
    auto rewritten = juce::parseXML(juce::String::fromUTF8(static_cast<const char*>(plain.getData()),
                                                           static_cast<int>(plain.getSize())));
    assert(rewritten != nullptr);

    juce::MemoryBlock binary;
    juce::AudioProcessor::copyXmlToBinary(*xml, binary);
    const auto binaryResult = fengyin::SwamExpressionCurve::applyToState(binary);
    assert(binaryResult.found);
    assert(! binaryResult.changed);
    assert(binaryResult.controller == 2);
    return 0;
}
