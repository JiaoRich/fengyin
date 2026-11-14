#pragma once

#include <array>
#include <atomic>
#include <cstdint>

namespace fengyin
{
inline constexpr int ewiSoloGlideSwitchCC = 65;
inline constexpr int ewiSoloGlideTimeCC = 5;
inline constexpr int ewiSoloLegatoTimeCC = 104;

// The switch has standard MIDI on/off values. Time controllers have two
// independently configurable values, which may be in either order.
struct EwiSoloGlideCalibration
{
    int controller = -1;
    int pressed = -1;
    int released = -1;

    bool valid() const noexcept
    {
        return controller == ewiSoloGlideSwitchCC
            || ((controller == ewiSoloGlideTimeCC || controller == ewiSoloLegatoTimeCC)
                && pressed >= 0 && pressed <= 127 && released >= 0 && released <= 127 && pressed != released);
    }
};

// Accessed under the short learning lock, never while calling the host.
class EwiSoloGlideLearning
{
public:
    bool observe(int channel, int controller, int value, double nowMs) noexcept
    {
        for (auto& sample : samples)
        {
            auto& calibration = sample.calibration;
            if (calibration.controller != controller) continue;
            if (calibration.pressed < 0)
            {
                if (controller == ewiSoloGlideSwitchCC && value < 64) return true;
                calibration.pressed = value;
                sample.channel = channel;
            }
            else if (channel == sample.channel && calibration.released < 0
                     && (controller == ewiSoloGlideSwitchCC ? value < 64 : value != calibration.pressed))
            {
                calibration.released = value;
                sample.releasedAtMs = nowMs;
            }
            return true;
        }
        return false;
    }

    bool started() const noexcept
    {
        for (const auto& sample : samples)
            if (sample.calibration.pressed >= 0) return true;
        return false;
    }

    EwiSoloGlideCalibration result(double nowMs) const noexcept
    {
        // Once the switch has been seen, wait for its own release. Its
        // accompanying time value cannot end the gesture.
        const auto& switchCalibration = samples.front().calibration;
        if (switchCalibration.pressed >= 0)
            return switchCalibration.released >= 0 ? switchCalibration : EwiSoloGlideCalibration {};
        for (const auto& sample : samples)
            if (sample.calibration.released >= 0 && nowMs - sample.releasedAtMs >= 50.0)
                return sample.calibration;
        return {};
    }

private:
    struct Sample
    {
        EwiSoloGlideCalibration calibration;
        int channel = 0;
        double releasedAtMs = 0.0;
    };
    std::array<Sample, 3> samples {{ {{ewiSoloGlideSwitchCC}}, {{ewiSoloGlideTimeCC}}, {{ewiSoloLegatoTimeCC}} }};
};

// The MIDI callback can publish the latest absolute switch state even when
// the technique lock is busy. The consumer drains it under that lock.
class EwiSoloGlideGate
{
public:
    void reset(EwiSoloGlideCalibration calibration = {}) noexcept
    {
        State next {};
        next.generation = static_cast<uint16_t>(state.load(std::memory_order_relaxed).generation + 1);
        if (calibration.controller >= 0 && calibration.controller <= 127)
        {
            next.controller = static_cast<uint8_t>(calibration.controller + 1);
            if (calibration.valid() && calibration.controller != ewiSoloGlideSwitchCC)
            {
                next.pressed = static_cast<uint8_t>(calibration.pressed);
                next.released = static_cast<uint8_t>(calibration.released);
            }
        }
        state.store(next, std::memory_order_release);
    }

    bool observe(int controller, int value) noexcept
    {
        auto current = state.load(std::memory_order_acquire);
        if (current.controller != controller + 1) return false;
        const auto generation = current.generation;
        auto techniqueValue = value;
        if (controller == ewiSoloGlideSwitchCC) techniqueValue = value >= 64 ? 127 : 0;
        if (current.pressed != uncalibrated)
        {
            if (value != current.pressed && value != current.released) return true;
            techniqueValue = value == current.pressed ? 127 : 0;
        }
        do
        {
            if (techniqueValue == current.value) return true;

            auto next = current;
            next.value = static_cast<uint8_t>(techniqueValue);
            next.pending = true;
            if (state.compare_exchange_strong(current, next, std::memory_order_acq_rel)) return true;
        } while (current.generation == generation);
        return true;
    }

    // Returns -1 when there is no update, otherwise the technique MIDI value.
    int consume() noexcept
    {
        auto current = state.load(std::memory_order_acquire);
        while (current.pending)
        {
            auto next = current;
            next.pending = false;
            if (state.compare_exchange_strong(current, next, std::memory_order_acq_rel)) return next.value;
        }
        return -1;
    }

    bool usesController(int controller) const noexcept
    {
        return state.load(std::memory_order_relaxed).controller == controller + 1;
    }

    bool isConfigured() const noexcept { return state.load(std::memory_order_relaxed).controller != 0; }

private:
    static constexpr uint8_t uncalibrated = 255;
    struct State
    {
        uint16_t generation = 0;
        uint8_t controller = 0, pressed = uncalibrated, released = 0, value = 0;
        uint16_t pending = false;
    };
    static_assert(sizeof(State) == 8);
    static_assert(std::atomic<State>::is_always_lock_free);
    std::atomic<State> state { State {} };
};
}
