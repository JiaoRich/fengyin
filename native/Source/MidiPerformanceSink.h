#pragma once

namespace fengyin
{
class MidiPerformanceSink
{
public:
    virtual ~MidiPerformanceSink() = default;
    virtual void noteOn(int noteNumber, float velocity) noexcept = 0;
    virtual void noteOff(int noteNumber) noexcept = 0;
    virtual void breathChanged(float value) noexcept = 0;
    virtual void pitchBendChanged(float bipolarValue) noexcept = 0;
};
}

