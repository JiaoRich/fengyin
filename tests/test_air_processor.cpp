#include "AirProcessor.h"
#include <cassert>
#include <iostream>
#include <limits>
#include <vector>

using Samples = std::vector<float>;
static Samples sine(double rate, double frequency, float amplitude)
{
    Samples signal(static_cast<std::size_t>(rate));
    for (std::size_t i = 0; i < signal.size(); ++i)
        signal[i] = amplitude * static_cast<float>(std::sin(i * frequency * 6.283185307179586 / rate));
    return signal;
}
static Samples render(const Samples& input, double rate, float amount, int block = 128)
{
    auto left = input, right = input;
    fengyin::AirProcessor processor;
    processor.prepare(rate);
    for (int offset = 0; offset < static_cast<int>(input.size()); offset += block)
    {
        float* outputs[] { left.data() + offset, right.data() + offset };
        processor.process(outputs, 2, std::min(block, static_cast<int>(input.size()) - offset), amount);
    }
    assert(left == right); // linked processing does not shift centred material
    for (auto sample : left) assert(std::isfinite(sample));
    return left;
}
static double rms(const Samples& input)
{
    double power = 0.0;
    for (auto i = input.size() / 2; i < input.size(); ++i) power += input[i] * input[i];
    return std::sqrt(power / (input.size() - input.size() / 2));
}
int main()
{
    for (auto rate : { 8000.0, 22050.0, 44100.0, 48000.0, 96000.0, 192000.0 })
    {
        const auto low = sine(rate, 100.0, 0.05f);
        const auto high = sine(rate, std::min(10000.0, rate * 0.35), 0.05f);
        assert(render(high, rate, 0.0f) == high); // old presets exactly bypass
        const auto enhanced = render(high, rate, 1.0f);
        assert(rms(enhanced) > rms(high) * 1.4);
        assert(rms(render(low, rate, 1.0f)) < rms(low) * 1.04);
        assert(rms(enhanced) > rms(render(high, rate, 0.5f)));
        assert(render(Samples(8192), rate, 1.0f) == Samples(8192)); // no added noise
        assert(enhanced == render(high, rate, 1.0f, 1));
        assert(enhanced == render(high, rate, 1.0f, 512));
        assert(render(high, rate, 5.0f) == enhanced);
        assert(render(high, rate, -1.0f) == high);
        assert(render(high, rate, std::numeric_limits<float>::quiet_NaN()) == high);
        const auto loud = sine(rate, std::min(10000.0, rate * 0.35), 0.8f);
        assert(rms(render(loud, rate, 1.0f)) / rms(loud) < rms(enhanced) / rms(high));
    }
    fengyin::AirProcessor processor;
    processor.prepare(48000.0);
    // Existing dry path responds in the first sample, never a deferred block.
    float impulse[] { 0.25f, 0.0f, 0.0f, 0.0f };
    float* mono[] { impulse };
    processor.process(mono, 1, 4, 1.0f);
    assert(impulse[0] > 0.25f && impulse[0] < 0.251f); // smoothed initial change
    auto tail = sine(48000.0, 3000.0, 0.1f);
    float* output[] { tail.data() };
    processor.process(output, 1, static_cast<int>(tail.size()), 0.0f);
    const auto dry = sine(48000.0, 3000.0, 0.1f);
    assert(std::equal(tail.end() - 1000, tail.end(), dry.end() - 1000));
    processor.reset();
    auto silence = Samples(1000);
    float* nullLane[] { nullptr, silence.data() };
    processor.process(nullLane, 2, 1000, 1.0f);
    assert(silence == Samples(1000));
    std::cout << "Air DSP: bypass, frequency response, stereo, dynamics, smoothing, block invariance and six sample rates passed\n";
}
