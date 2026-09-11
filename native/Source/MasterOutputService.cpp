#include "MasterOutputService.h"
#include <cmath>
namespace fengyin
{
void MasterOutputService::process(float* const* outputs, int channels, int samples) noexcept
{
    if (outputs == nullptr || channels <= 0 || samples <= 0) return;
    const auto currentGain = gain.load(std::memory_order_relaxed);
    float peaks[2] {};
    for (int channel = 0; channel < channels; ++channel)
    {
        auto* data = outputs[channel];
        if (data == nullptr) continue;
        float peak = 0.0f;
        for (int sample = 0; sample < samples; ++sample)
        {
            data[sample] *= currentGain;
            peak = juce::jmax(peak, std::abs(data[sample]));
        }
        peaks[juce::jmin(channel, 1)] = juce::jmax(peaks[juce::jmin(channel, 1)], peak);
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
