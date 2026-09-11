#pragma once
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <atomic>
namespace fengyin
{
class MasterOutputService
{
public:
    static constexpr int spectrumBands = 54;
    void setGain(float value) noexcept { gain.store(juce::jlimit(0.0f, 1.5f, value)); }
    void setEqTone(float value) noexcept { eqTone.store(juce::jlimit(-1.0f, 1.0f, value)); }
    void setReverbMix(float value) noexcept { reverbMix.store(juce::jlimit(0.0f, 0.6f, value)); }
    void setLimiterCeiling(float value) noexcept { limiterCeiling.store(juce::jlimit(0.6f, 1.0f, value)); }
    float getGain() const noexcept { return gain.load(); }
    float getLeftPeak() const noexcept { return leftPeak.load(); }
    float getRightPeak() const noexcept { return rightPeak.load(); }
    void setSampleRate(double value) noexcept { sampleRate.store(value > 0.0 ? value : 48000.0); }
    void process(float* const* outputs, int channels, int samples) noexcept;
    bool getSpectrum(std::array<float, spectrumBands>& result);
private:
    static constexpr std::size_t fifoSize = 8192;
    static constexpr int fftOrder = 11;
    static constexpr int fftSize = 1 << fftOrder;
    std::array<float, fifoSize> fifo {};
    std::atomic<std::size_t> writeIndex { 0 }, readIndex { 0 };
    std::atomic<float> gain { 0.8f }, leftPeak { 0.0f }, rightPeak { 0.0f };
    // Neutral until the web controls publish their visible defaults.
    std::atomic<float> eqTone { 0.0f }, reverbMix { 0.0f }, limiterCeiling { 1.0f };
    std::atomic<double> sampleRate { 48000.0 };
    static constexpr std::size_t reverbCapacity = 96000;
    std::array<std::array<float, reverbCapacity>, 2> reverbDelay {};
    std::array<float, 2> lowPassState {};
    std::size_t reverbWriteIndex = 0;
    juce::dsp::FFT fft { fftOrder };
    juce::dsp::WindowingFunction<float> window { fftSize, juce::dsp::WindowingFunction<float>::hann };
    std::array<float, fftSize * 2> fftData {};
};
}
