#include "MasterOutputService.h"
#include <cmath>
namespace fengyin
{
void MasterOutputService::process(float* const* outputs, int channels, int samples) noexcept
{
    if (outputs == nullptr || channels <= 0 || samples <= 0) return;
    const auto currentGain = gain.load(std::memory_order_relaxed);
    const auto tone = eqTone.load(std::memory_order_relaxed);
    const auto wetMix = reverbMix.load(std::memory_order_relaxed);
    const auto ceiling = limiterCeiling.load(std::memory_order_relaxed);
    const auto delayLength = juce::jlimit<std::size_t>(1, reverbCapacity,
        static_cast<std::size_t>(sampleRate.load(std::memory_order_relaxed) * 0.115));
    reverbWriteIndex %= delayLength;
    float peaks[2] {};
    for (int sample = 0; sample < samples; ++sample)
    {
        for (int channel = 0; channel < channels; ++channel)
        {
            auto* data = outputs[channel];
            if (data == nullptr) continue;
            const auto lane = static_cast<std::size_t>(juce::jmin(channel, 1));
            auto value = data[sample] * currentGain;
            lowPassState[lane] += 0.08f * (value - lowPassState[lane]);
            value += tone >= 0.0f ? tone * 0.35f * (value - lowPassState[lane])
                                  : (-tone) * 0.25f * lowPassState[lane];
            const auto delayed = reverbDelay[lane][reverbWriteIndex];
            reverbDelay[lane][reverbWriteIndex] = value + delayed * 0.34f;
            value = value * (1.0f - wetMix) + delayed * wetMix;
            value = juce::jlimit(-ceiling, ceiling, value);
            data[sample] = value;
            peaks[lane] = juce::jmax(peaks[lane], std::abs(value));
        }
        reverbWriteIndex = (reverbWriteIndex + 1) % delayLength;
    }
    leftPeak.store(peaks[0], std::memory_order_relaxed);
    rightPeak.store(channels > 1 ? peaks[1] : peaks[0], std::memory_order_relaxed);
    if (outputs[0] == nullptr) return;
    auto write = writeIndex.load(std::memory_order_relaxed);
    const auto read = readIndex.load(std::memory_order_acquire);
    for (int sample = 0; sample < samples; ++sample)
    {
        const auto next = (write + 1) % fifoSize;
        if (next == read) break;
        fifo[write] = outputs[0][sample];
        write = next;
    }
    writeIndex.store(write, std::memory_order_release);
}

bool MasterOutputService::getSpectrum(std::array<float, spectrumBands>& result)
{
    auto read = readIndex.load(std::memory_order_relaxed);
    const auto write = writeIndex.load(std::memory_order_acquire);
    int available = write >= read ? static_cast<int>(write - read) : static_cast<int>(fifoSize - read + write);
    if (available < fftSize) return false;
    while (available > fftSize) { read = (read + 1) % fifoSize; --available; }
    fftData.fill(0.0f);
    for (int i = 0; i < fftSize; ++i)
    {
        fftData[static_cast<size_t>(i)] = fifo[read];
        read = (read + 1) % fifoSize;
    }
    readIndex.store(read, std::memory_order_release);
    window.multiplyWithWindowingTable(fftData.data(), fftSize);
    fft.performFrequencyOnlyForwardTransform(fftData.data());
    const auto rate = sampleRate.load(std::memory_order_relaxed);
    for (int band = 0; band < spectrumBands; ++band)
    {
        const auto proportion = static_cast<double>(band) / static_cast<double>(spectrumBands - 1);
        const auto frequency = 45.0 * std::pow(18000.0 / 45.0, proportion);
        const auto bin = juce::jlimit(1, fftSize / 2 - 1, juce::roundToInt(frequency * fftSize / rate));
        const auto magnitude = fftData[static_cast<size_t>(bin)] / static_cast<float>(fftSize);
        result[static_cast<size_t>(band)] = juce::jlimit(0.0f, 1.0f,
            (juce::Decibels::gainToDecibels(magnitude, -80.0f) + 80.0f) / 80.0f);
    }
    return true;
}
}
