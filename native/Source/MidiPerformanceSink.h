#pragma once

#include <string_view>

namespace fengyin
{
enum class PerformanceTechnique
{
    growl = 0,
    vibrato,
    flutter,
    portamento,
    fall,
    overblow,
    breathNoise,
    alternateFingering,
    mute,
    halfValve,
    legato,
    bowPressure,
    pizzicato,
    tremolo,
    count
};

enum class TechniqueControlMode
{
    automatic = 0,
    breath,
    hardware,
    hybrid,
    off
};

enum class TechniqueSourceType
{
    none = 0,
    controller,
    channelPressure,
    pitchWheel,
    note
};

struct TechniqueMapping
{
    PerformanceTechnique technique = PerformanceTechnique::growl;
    TechniqueSourceType sourceType = TechniqueSourceType::none;
    int sourceNumber = -1;
    bool toggle = false;
    TechniqueControlMode mode = TechniqueControlMode::automatic;
    float strength = 0.5f;
};

[[nodiscard]] inline const char* techniqueId(PerformanceTechnique technique) noexcept
{
    switch (technique)
    {
        case PerformanceTechnique::growl: return "growl";
        case PerformanceTechnique::vibrato: return "vibrato";
        case PerformanceTechnique::flutter: return "flutter";
        case PerformanceTechnique::portamento: return "portamento";
        case PerformanceTechnique::fall: return "fall";
        case PerformanceTechnique::overblow: return "overblow";
        case PerformanceTechnique::breathNoise: return "breathNoise";
        case PerformanceTechnique::alternateFingering: return "altFingering";
        case PerformanceTechnique::mute: return "mute";
        case PerformanceTechnique::halfValve: return "halfValve";
        case PerformanceTechnique::legato: return "legato";
        case PerformanceTechnique::bowPressure: return "bowPressure";
        case PerformanceTechnique::pizzicato: return "pizzicato";
        case PerformanceTechnique::tremolo: return "tremolo";
        case PerformanceTechnique::count: break;
    }
    return "unknown";
}

[[nodiscard]] inline PerformanceTechnique techniqueFromId(const std::string_view id) noexcept
{
    for (int value = 0; value < static_cast<int>(PerformanceTechnique::count); ++value)
    {
        const auto technique = static_cast<PerformanceTechnique>(value);
        if (id == techniqueId(technique)) return technique;
    }
    return PerformanceTechnique::count;
}

class MidiPerformanceSink
{
public:
    virtual ~MidiPerformanceSink() = default;
    // timestampSeconds uses the monotonic timestamp supplied by the MIDI backend.
    // Keeping it all the way to the audio callback allows sample-offset scheduling
    // instead of quantising every event to the start of the next audio block.
    virtual void noteOn(int noteNumber, float velocity, double timestampSeconds = 0.0) noexcept = 0;
    virtual void noteOff(int noteNumber, double timestampSeconds = 0.0) noexcept = 0;
    virtual void breathChanged(float value, double timestampSeconds = 0.0) noexcept = 0;
    virtual void pitchBendChanged(float bipolarValue, double timestampSeconds = 0.0) noexcept = 0;
    virtual void techniqueChanged(PerformanceTechnique, float) noexcept {}
    virtual void resetPerformance() noexcept {}
};
}
