#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <cmath>
#include <optional>

namespace fengyin {
// Kenny G PRO profile only. User-assigned Host Automation #0001 -> percent.
// The supplied KSP declares Pitch Sensitivity 10..400 (integer percent);
// the exported 100% state reads (100-10)/390 = 0.2307692319.
// Requires the profile's saved Use Windcontroller ON. Do not apply to arbitrary
// Kontakt libraries or interpret the MIDI PitchBend value as a range parameter.
struct KensaBendRange {
    static std::optional<float> valueFor(int semitones) noexcept {
        if(semitones<1 || semitones>4)return {};
        return (static_cast<float>(semitones*100)-10.f)/390.f;
    }
    static juce::AudioProcessorParameter* boundParameter(juce::AudioProcessor& plugin) {
        const auto& parameters=plugin.getParameters();
        if(parameters.size()<2)return nullptr;
        auto* p=parameters[1];
        return p && p->getName(160).trim().equalsIgnoreCase("percent") ? p : nullptr;
    }
    static bool set(juce::AudioProcessor& plugin,int semitones) {
        const auto target=valueFor(semitones);auto* p=boundParameter(plugin);
        if(!target || !p)return false;
        const float old=p->getValue();
        p->beginChangeGesture();p->setValueNotifyingHost(*target);p->endChangeGesture();
        const auto actual=p->getValue();
        if(std::isfinite(actual) && std::abs(actual-*target)<.0001f)return true;
        p->setValueNotifyingHost(old);return false;
    }
};
}
