#pragma once
namespace fengyin::audioengine
{
// Fixed gains retain breath dynamics: no AGC, gate, or accompaniment pumping.
inline constexpr float performanceGain = 1.41253754f; // +3 dB, before instrument ceiling
inline constexpr float accompanimentGain = 0.70794578f; // -3 dB, only inside FengYin
}
