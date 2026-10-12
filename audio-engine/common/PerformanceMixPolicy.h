#pragma once
namespace fengyin::audioengine
{
// Unity buses: no fixed makeup gain, accompaniment attenuation or AGC.
inline constexpr float performanceGain = 1.0f;
inline constexpr float accompanimentGain = 1.0f;
}
