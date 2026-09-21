#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>

#include "MidiPerformanceSink.h"

namespace fengyin
{
// 将已经校准到 0..1 的气息转换为乐器技巧。它只处理适合气息驱动的连续技巧；
// 离散技巧（弱音器、拨奏等）仍交给真实按键，避免强吹时误触发。
class IntelligentTechniqueProcessor
{
public:
    void reset() noexcept
    {
        for (auto& state : states) state = {};
        noteActive = false;
        noteStartedAtMs = 0.0;
    }

    void noteStarted(double nowMs) noexcept
    {
        noteActive = true;
        noteStartedAtMs = nowMs;
        // 换音不重置嘶吼判定，避免中等气息在音符切换瞬间被重新判为一次强吹。
        for (size_t index = 0; index < states.size(); ++index)
            if (index != static_cast<size_t>(PerformanceTechnique::growl))
                states[index].aboveSinceMs = 0.0;
    }

    void noteEnded() noexcept { noteActive = false; }

    void setGrowlThresholds(float onThreshold, float offThreshold) noexcept
    {
        growlOnThreshold.store(std::clamp(onThreshold, 0.75f, 0.98f), std::memory_order_relaxed);
        growlOffThreshold.store(std::clamp(offThreshold, 0.65f, 0.94f), std::memory_order_relaxed);
    }

    [[nodiscard]] static bool canUseBreath(PerformanceTechnique technique) noexcept
    {
        switch (technique)
        {
            case PerformanceTechnique::vibrato:
            case PerformanceTechnique::growl:
            case PerformanceTechnique::flutter:
            case PerformanceTechnique::overblow:
            case PerformanceTechnique::breathNoise:
            case PerformanceTechnique::bowPressure:
                return true;
            case PerformanceTechnique::portamento:
            case PerformanceTechnique::fall:
            case PerformanceTechnique::alternateFingering:
            case PerformanceTechnique::mute:
            case PerformanceTechnique::halfValve:
            case PerformanceTechnique::legato:
            case PerformanceTechnique::pizzicato:
            case PerformanceTechnique::tremolo:
            case PerformanceTechnique::count:
                return false;
        }
    }

    float process(PerformanceTechnique technique, float breath, float strength,
                  double nowMs) noexcept
    {
        const auto index = static_cast<size_t>(technique);
        if (index >= states.size() || ! canUseBreath(technique)) return 0.0f;
        auto& state = states[index];
        const auto safeBreath = std::clamp(breath, 0.0f, 1.0f);
        const auto safeStrength = std::clamp(strength, 0.0f, 1.0f);
        const auto elapsed = state.lastUpdateMs > 0.0 ? std::clamp(nowMs - state.lastUpdateMs, 1.0, 100.0) : 10.0;
        state.lastUpdateMs = nowMs;

        if (! noteActive)
        {
            state.latched = false;
            state.aboveSinceMs = 0.0;
            return smooth(state, 0.0f, elapsed, 70.0f);
        }

        if (technique == PerformanceTechnique::vibrato)
        {
            // 长音先保持自然直音，再在约半秒后缓慢加入颤音。弱吹时减少深度，
            // 避免每个音一开始就有机械颤音，也不会抢占带咬合传感器的硬件控制。
            const auto noteAge = std::max(0.0, nowMs - noteStartedAtMs);
            const auto fadeIn = std::clamp(static_cast<float>((noteAge - 420.0) / 650.0), 0.0f, 1.0f);
            const auto breathWeight = std::clamp((safeBreath - 0.12f) / 0.58f, 0.0f, 1.0f);
            const auto target = fadeIn * breathWeight * (0.12f + safeStrength * 0.46f);
            return smooth(state, target, elapsed, target > state.value ? 210.0f : 130.0f);
        }

        if (technique == PerformanceTechnique::breathNoise)
        {
            const auto target = std::clamp((0.42f - safeBreath) / 0.42f, 0.0f, 1.0f)
                              * (0.18f + 0.42f * safeStrength);
            return smooth(state, target, elapsed, target > state.value ? 110.0f : 170.0f);
        }
        if (technique == PerformanceTechnique::bowPressure)
        {
            const auto target = std::pow(safeBreath, 1.15f) * (0.35f + 0.65f * safeStrength);
            return smooth(state, target, elapsed, 90.0f);
        }

        const auto curve = curveFor(technique, safeStrength);
        if (safeBreath >= curve.onThreshold)
        {
            if (state.aboveSinceMs <= 0.0) state.aboveSinceMs = nowMs;
            const auto noteOldEnough = nowMs - noteStartedAtMs >= curve.onsetGuardMs;
            if (noteOldEnough && nowMs - state.aboveSinceMs >= curve.holdMs)
                state.latched = true;
        }
        else if (safeBreath <= curve.offThreshold)
        {
            state.latched = false;
            state.aboveSinceMs = 0.0;
        }

        auto target = 0.0f;
        if (state.latched)
        {
            const auto normalised = std::clamp((safeBreath - curve.offThreshold)
                                             / (1.0f - curve.offThreshold), 0.0f, 1.0f);
            target = std::pow(normalised, 1.15f) * curve.maximum;
        }
        return smooth(state, target, elapsed, target > state.value ? curve.attackMs : curve.releaseMs);
    }

private:
    struct Curve
    {
        float onThreshold;
        float offThreshold;
        float maximum;
        double holdMs;
        double onsetGuardMs;
        float attackMs;
        float releaseMs;
    };

    struct State
    {
        float value = 0.0f;
        bool latched = false;
        double aboveSinceMs = 0.0;
        double lastUpdateMs = 0.0;
    };

    [[nodiscard]] Curve curveFor(PerformanceTechnique technique, float strength) const noexcept
    {
        // 嘶吼的触发阈值与“效果强度”彻底分离：强度只决定音色深浅，
        // 不再让同一次吹奏因为调了强度而改变触发位置。
        const auto thresholdShift = (strength - 0.5f) * 0.18f;
        if (technique == PerformanceTechnique::flutter)
            return { std::clamp(0.95f - thresholdShift, 0.72f, 0.98f), 0.84f,
                     0.55f + strength * 0.35f, 150.0, 150.0, 90.0f, 150.0f };
        if (technique == PerformanceTechnique::overblow)
            return { std::clamp(0.96f - thresholdShift, 0.76f, 0.99f), 0.88f,
                     0.50f + strength * 0.45f, 80.0, 90.0, 65.0f, 120.0f };
        return { growlOnThreshold.load(std::memory_order_relaxed),
                 growlOffThreshold.load(std::memory_order_relaxed),
                 std::min(1.0f, 0.42f + strength * 0.60f),
                 60.0, 0.0, 85.0f, 170.0f };
    }

    static float smooth(State& state, float target, double elapsedMs, float timeMs) noexcept
    {
        const auto alpha = 1.0f - std::exp(-static_cast<float>(elapsedMs) / std::max(1.0f, timeMs));
        state.value += (target - state.value) * alpha;
        if (state.value < 0.0001f) state.value = 0.0f;
        return state.value;
    }

    std::array<State, static_cast<size_t>(PerformanceTechnique::count)> states {};
    std::atomic<float> growlOnThreshold { 0.89f };
    std::atomic<float> growlOffThreshold { 0.80f };
    bool noteActive = false;
    double noteStartedAtMs = 0.0;
};
}
