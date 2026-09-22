#pragma once

#include <juce_dsp/juce_dsp.h>
#include <array>
#include <atomic>

namespace fengyin
{
enum class InstrumentMixProfile { generic = 0, saxophone, brass, woodwind, strings };

struct ToneStyleSettings
{
    float tone = 0.0f;
    float warmth = 0.2f;
    float reverbMix = 0.2f;
    float compressionThreshold = 0.58f;
    float compressionRatio = 1.7f;
    float saturation = 0.04f;
    float harshControl = 0.10f;
    float reverbRoomSize = 0.42f;
    float reverbDamping = 0.54f;
    float reverbWidth = 0.88f;
    float outputGain = 1.0f;
};

class MasterOutputService
{
public:
    static constexpr int spectrumBands = 54;

    void setGain(float value) noexcept { gain.store(juce::jlimit(0.0f, 1.5f, value)); }
    void setEqTone(float value) noexcept { eqTone.store(juce::jlimit(-1.0f, 1.0f, value)); }
    void setWarmth(float value) noexcept { warmth.store(juce::jlimit(0.0f, 1.0f, value)); }
    void setReverbMix(float value) noexcept { reverbMix.store(juce::jlimit(0.0f, 0.6f, value)); }
    void setLimiterCeiling(float value) noexcept { limiterCeiling.store(juce::jlimit(0.6f, 1.0f, value)); }
    void setSmartOptimisationEnabled(bool enabled) noexcept { smartOptimisation.store(enabled); }
    void setInstrumentProfile(InstrumentMixProfile value) noexcept { instrumentProfile.store(static_cast<int>(value)); }
    void setToneStyle(const ToneStyleSettings& settings) noexcept;

    [[nodiscard]] float getGain() const noexcept { return gain.load(); }
    [[nodiscard]] float getEqTone() const noexcept { return eqTone.load(); }
    [[nodiscard]] float getReverbMix() const noexcept { return reverbMix.load(); }
    [[nodiscard]] float getLimiterCeiling() const noexcept { return limiterCeiling.load(); }
    [[nodiscard]] bool isSmartOptimisationEnabled() const noexcept { return smartOptimisation.load(); }
    [[nodiscard]] float getWarmth() const noexcept { return warmth.load(); }
    [[nodiscard]] ToneStyleSettings getToneStyle() const noexcept;
    [[nodiscard]] float getLeftPeak() const noexcept { return leftPeak.load(); }
    [[nodiscard]] float getRightPeak() const noexcept { return rightPeak.load(); }

    void setSampleRate(double value) noexcept;
    void processInstrument(float* const* outputs, int channels, int samples) noexcept;
    void processMaster(float* const* outputs, int channels, int samples) noexcept;
    void process(float* const* outputs, int channels, int samples) noexcept { processMaster(outputs, channels, samples); }
    bool getSpectrum(std::array<float, spectrumBands>& result);

private:
    struct ProfileSettings
    {
        float toneBias = 0.0f;
        float compressionThreshold = 0.58f;
        float compressionRatio = 1.7f;
        float reverbRoomSize = 0.42f;
        float reverbDamping = 0.54f;
        float reverbScale = 1.0f;
    };

    [[nodiscard]] ProfileSettings currentProfileSettings() const noexcept;
    void updateInstrumentReverb(float mix, const ProfileSettings& settings) noexcept;
    void updateSpectrumAndPeaks(float* const* outputs, int channels, int samples) noexcept;

    static constexpr std::size_t fifoSize = 8192;
    static constexpr int fftOrder = 11;
    static constexpr int fftSize = 1 << fftOrder;
    std::array<float, fifoSize> fifo {};
    std::atomic<std::size_t> writeIndex { 0 }, readIndex { 0 };
    std::atomic<float> gain { 0.8f }, leftPeak { 0.0f }, rightPeak { 0.0f };
    std::atomic<float> eqTone { 0.0f }, reverbMix { 0.0f }, limiterCeiling { 1.0f };
    std::atomic<float> warmth { 0.2f }, styleCompressionThreshold { 0.58f }, styleCompressionRatio { 1.7f };
    std::atomic<float> styleSaturation { 0.04f }, styleHarshControl { 0.10f };
    std::atomic<float> styleRoomSize { 0.42f }, styleDamping { 0.54f }, styleWidth { 0.88f };
    std::atomic<float> styleOutputGain { 1.0f };
    std::atomic<bool> smartOptimisation { true };
    std::atomic<int> instrumentProfile { static_cast<int>(InstrumentMixProfile::generic) };
    std::atomic<double> sampleRate { 48000.0 };

    juce::Reverb instrumentReverb;
    float lastInstrumentReverbMix = -1.0f;
    int lastInstrumentProfile = -1;
    float instrumentEnvelope = 0.0f;
    float compressorGain = 1.0f;
    float automaticTrim = 1.0f;
    std::array<float, 2> toneLowPass {};
    std::array<float, 2> rumbleLowPass {};

    juce::dsp::FFT fft { fftOrder };
    juce::dsp::WindowingFunction<float> window { fftSize, juce::dsp::WindowingFunction<float>::hann };
    std::array<float, fftSize * 2> fftData {};
};
}
