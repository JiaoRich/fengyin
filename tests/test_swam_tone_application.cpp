#include "SwamToneApplication.h"
#include <cassert>
#include <memory>

// Model plugin display-domain conversion independently of the profile values.
class Parameter final : public juce::AudioProcessorParameter
{
public:
    Parameter(juce::String n, float a, float b, bool toggle = false)
        : name(n), low(a), high(b), boolean(toggle) {}
    float getValue() const override { return value; }
    void setValue(float v) override { ++writes; if (! ignoreWrites) value = v; }
    float getDefaultValue() const override { return 0.5f; }
    juce::String getName(int) const override { return name; }
    juce::String getLabel() const override { return {}; }
    int getNumSteps() const override { return boolean ? 2 : juce::AudioProcessor::getDefaultNumParameterSteps(); }
    bool isDiscrete() const override { return boolean; }
    juce::String getText(float v, int) const override
    {
        if (boolean) return v > 0.5f ? "ON" : "OFF";
        const auto mapped = nonlinear ? v * v : v;
        return juce::String(low + (high - low) * mapped, 3);
    }
    float getValueForText(const juce::String& text) const override
    {
        if (brokenParser) return 0;
        if (boolean) return text == "ON" ? 1.0f : 0.0f;
        const float linear = juce::jlimit(0.0f, 1.0f, (text.getFloatValue() - low) / (high - low));
        return nonlinear ? std::sqrt(linear) : linear;
    }
    juce::String name;
    float low, high, value = 0.5f;
    bool boolean = false, nonlinear = false, brokenParser = false, ignoreWrites = false;
    int writes = 0;
};

int main()
{
    using A = fengyin::SwamToneApplication;
    assert(! A::number("OFF")); assert(! A::number("3 frogs"));
    assert(! A::matches("nan", "0")); assert(! A::matches("0.00", "-0.08"));
    assert(A::matches("1.20 dB", "1.2"));
    Parameter nonlinear("EQ Mid Freq", 100, 10000);
    nonlinear.nonlinear = nonlinear.brokenParser = true;
    const auto n = A::resolve(nonlinear, "2500");
    assert(n && A::matches(nonlinear.getText(*n, 256), "2500"));
    assert(! A::resolve(nonlinear, "20000"));
    Parameter reverse("Formant", 5, -5); reverse.brokenParser = true;
    assert(A::resolve(reverse, "-0.8"));

    std::vector<std::unique_ptr<Parameter>> owned;
    juce::Array<juce::AudioProcessorParameter*> params;
    const auto profile = fengyin::SwamToneProfile::soprano(0);
    for (const auto& target : profile.displayTargets)
    {
        const juce::String name(target.name);
        const bool toggle = name == "Timbral Correction" || name == "EQ Enabled" || name == "Breathy ppp";
        float lo = 0, hi = 127;
        if (name == "Harmonic Structure") { lo = -0.15f; hi = 0.15f; }
        else if (name == "Formant") { lo = -7; hi = 7; }
        else if (name.contains("Gain") && name != "Modal Res. Gain") { lo = -12; hi = 12; }
        else if (name == "EQ Mid Freq") { lo = 100; hi = 10000; }
        else if (name == "Dynamic Harmonic") { lo = 0; hi = 1; }
        auto p = std::make_unique<Parameter>(name, lo, hi, toggle);
        p->brokenParser = true; // exercise safe fallback, like limited VST wrappers
        params.add(p.get()); owned.push_back(std::move(p));
    }
    const auto targetCount = params.size();
    for (const auto* name : {"Expression", "MIDI CC 0|11", "Pitch Bend Up", "Growl", "Instrument", "Vibrato Depth"})
    {
        auto p = std::make_unique<Parameter>(name, 0, 127);
        params.add(p.get()); owned.push_back(std::move(p));
    }
    A application;
    for (int pass = 0; pass < 4; ++pass)
        for (int style : {1, 2, 0})
        {
            const auto requested = fengyin::SwamToneProfile::soprano(style);
            assert(application.apply(params, requested) == 22);
            for (int i = 0; i < targetCount; ++i)
                assert(A::matches(params[i]->getCurrentValueAsText(), requested.displayTargets[static_cast<size_t>(i)].display));
            for (int i = targetCount; i < params.size(); ++i)
                assert(owned[static_cast<size_t>(i)]->writes == 0);
        }
    // An unsupported or ambiguous parameter cannot leave a half-applied tone.
    const float before = params[0]->getValue();
    auto missing = params; missing.remove(3);
    assert(application.apply(missing, fengyin::SwamToneProfile::soprano(1)) == 0);
    assert(params[0]->getValue() == before);
    auto duplicate = params; duplicate.add(params[0]);
    assert(application.apply(duplicate, profile) == 0);
    auto invalid = profile; invalid.displayTargets[0].display = "9999";
    assert(application.apply(params, invalid) == 0);
    assert(params[0]->getValue() == before);
    // A plugin refusing writes triggers rollback, not a false success count.
    owned[3]->ignoreWrites = true;
    assert(application.apply(params, fengyin::SwamToneProfile::soprano(1)) == 0);
    assert(application.rolledBack && params[0]->getValue() == before);
    const auto report = application.diagnostic();
    assert(static_cast<int>(report["expected"]) == 22);
    assert(static_cast<int>(report["verifiedAtApply"]) == 0);
    assert(report["parameters"].getArray()->size() == 22);
}
