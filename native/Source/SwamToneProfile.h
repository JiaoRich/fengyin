#pragma once

namespace fengyin
{
// Values are normalised VST parameters.  This profile intentionally contains
// acoustic/timbre controls only: MIDI mapping, expression, breath, pitch bend
// and performance-technique parameters are never part of a tone style.
struct SwamToneProfile
{
    bool enabled = false;
    float timbre = 0.5f;
    float brightness = 0.5f;
    float formant = 0.5f;
    float reedStiffness = 0.5f;
    float attack = 0.5f;
    float harmonics = 0.5f;
    float keyNoise = 0.15f;
    float resonance = 0.5f;
    float breathNoise = 0.5f;
};
}
