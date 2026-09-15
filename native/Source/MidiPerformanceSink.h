#pragma once

namespace fengyin
{
enum class PerformanceTechnique
{
    growl = 0,
    vibrato,
    flutter,
    count
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
};

class MidiPerformanceSink
{
public:
    virtual ~MidiPerformanceSink() = default;
    virtual void noteOn(int noteNumber, float velocity) noexcept = 0;
    virtual void noteOff(int noteNumber) noexcept = 0;
    virtual void breathChanged(float value) noexcept = 0;
    virtual void pitchBendChanged(float bipolarValue) noexcept = 0;
    virtual void techniqueChanged(PerformanceTechnique, float) noexcept {}
    virtual void resetPerformance() noexcept {}
};
}
