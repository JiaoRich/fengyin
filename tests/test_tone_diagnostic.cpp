#include "ToneDiagnostic.h"
#include <cassert>
#include <cmath>

class DiagnosticFixture final : public juce::AudioProcessorGraph
{
public:
    DiagnosticFixture()
    {
        addParameter(new juce::AudioParameterFloat("brightness", "Brightness", 0.0f, 1.0f, 0.35f));
        addParameter(new juce::AudioParameterFloat("routing", "MIDI Controller", 0.0f, 127.0f, 2.0f));
        addParameter(new juce::AudioParameterFloat("unusual", "Unknown Vendor Control", 0.0f, 1.0f, 0.75f));
    }
    void getStateInformation(juce::MemoryBlock& state) override
    {
        ++readCount;
        if (! emptyState) { const unsigned char bytes[] { 0, 1, 2, 127, 255, 0 }; state.replaceAll(bytes, sizeof(bytes)); }
    }
    void setStateInformation(const void*, int) override { ++writeCount; }
    int readCount = 0, writeCount = 0;
    bool emptyState = false;
};

int main()
{
    juce::ScopedJuceInitialiser_GUI initialise;
    DiagnosticFixture processor;
    const auto before = processor.getParameters()[0]->getValue();
    const auto report = fengyin::ToneDiagnostic::capture(processor);
    assert(processor.readCount == 1 && processor.writeCount == 0);
    assert(std::abs(processor.getParameters()[0]->getValue() - before) < 1.0e-7f);
    assert(static_cast<int>(report["parameterCount"]) == 3);
    const auto* rows = report["parameters"].getArray();
    assert(rows != nullptr && rows->size() == 3);
    assert((*rows)[0]["nameChinese"].toString() == juce::String::fromUTF8("明亮度"));
    assert((*rows)[1]["id"].toString() == "routing"); // MIDI routing is not filtered out.
    assert((*rows)[2]["nameEnglish"].toString() == "Unknown Vendor Control");
    assert(! static_cast<bool>((*rows)[2]["translationVerified"]));
    juce::MemoryOutputStream decoded;
    assert(juce::Base64::convertFromBase64(decoded, report["stateBase64"].toString()));
    assert(decoded.getDataSize() == 6);
    assert(static_cast<const unsigned char*>(decoded.getData())[4] == 255);
    const auto roundTrip = juce::JSON::parse(juce::JSON::toString(report));
    assert(roundTrip["stateBase64"] == report["stateBase64"]);
    // JSON decimal formatting may round the double representation of a float.
    assert(std::abs(static_cast<double>((*roundTrip["parameters"].getArray())[0]["normalisedValue"])
                    - static_cast<double>(before)) < 1.0e-7);
    processor.emptyState = true;
    const auto empty = fengyin::ToneDiagnostic::capture(processor);
    assert(! static_cast<bool>(empty["stateAvailable"]));
    assert(static_cast<int>(empty["parameterCount"]) == 3);
}
