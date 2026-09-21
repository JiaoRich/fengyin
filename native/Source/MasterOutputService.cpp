#include "MasterOutputService.h"

#include <cmath>

namespace fengyin
{
void MasterOutputService::setToneStyle(const ToneStyleSettings& settings) noexcept
{
    eqTone.store(juce::jlimit(-1.0f, 1.0f, settings.tone));
    warmth.store(juce::jlimit(0.0f, 1.0f, settings.warmth));
    reverbMix.store(juce::jlimit(0.0f, 0.6f, settings.reverbMix));
    styleCompressionThreshold.store(juce::jlimit(0.20f, 0.95f, settings.compressionThreshold));
    styleCompressionRatio.store(juce::jlimit(1.0f, 4.0f, settings.compressionRatio));
    styleSaturation.store(juce::jlimit(0.0f, 0.35f, settings.saturation));
    styleHarshControl.store(juce::jlimit(0.0f, 0.35f, settings.harshControl));
    styleRoomSize.store(juce::jlimit(0.0f, 1.0f, settings.reverbRoomSize));
    styleDamping.store(juce::jlimit(0.0f, 1.0f, settings.reverbDamping));
    styleWidth.store(juce::jlimit(0.0f, 1.0f, settings.reverbWidth));
    styleOutputGain.store(juce::jlimit(0.5f, 1.25f, settings.outputGain));
    lastInstrumentReverbMix = -1.0f;
}

ToneStyleSettings MasterOutputService::getToneStyle() const noexcept
{
    return { eqTone.load(), warmth.load(), reverbMix.load(), styleCompressionThreshold.load(),
             styleCompressionRatio.load(), styleSaturation.load(), styleHarshControl.load(),
             styleRoomSize.load(), styleDamping.load(), styleWidth.load(), styleOutputGain.load() };
}

void MasterOutputService::setSampleRate(double value) noexcept
{
    const auto validRate = value > 0.0 ? value : 48000.0;
    sampleRate.store(validRate, std::memory_order_relaxed);
    instrumentReverb.setSampleRate(validRate);
    glueReverb.setSampleRate(validRate);
    juce::Reverb::Parameters glue;
    glue.roomSize = 0.18f;
    glue.damping = 0.72f;
    glue.wetLevel = 0.035f;
    glue.dryLevel = 0.965f;
    glue.width = 0.72f;
    glueReverb.setParameters(glue);
    instrumentReverb.reset();
    glueReverb.reset();
    lastInstrumentReverbMix = -1.0f;
}

MasterOutputService::ProfileSettings MasterOutputService::currentProfileSettings() const noexcept
{
    switch (static_cast<InstrumentMixProfile>(instrumentProfile.load(std::memory_order_relaxed)))
    {
        case InstrumentMixProfile::saxophone: return { -0.08f, 0.56f, 1.65f, 0.46f, 0.56f, 1.0f };
        case InstrumentMixProfile::brass:     return { -0.12f, 0.52f, 1.85f, 0.38f, 0.62f, 0.82f };
        case InstrumentMixProfile::woodwind:  return { -0.03f, 0.60f, 1.55f, 0.50f, 0.58f, 1.08f };
        case InstrumentMixProfile::strings:   return {  0.02f, 0.62f, 1.50f, 0.56f, 0.52f, 1.14f };
        case InstrumentMixProfile::generic:   break;
    }
    return {};
}

void MasterOutputService::updateInstrumentReverb(float mix, const ProfileSettings& settings) noexcept
{
    const auto profile = instrumentProfile.load(std::memory_order_relaxed);
    if (std::abs(mix - lastInstrumentReverbMix) < 0.001f && profile == lastInstrumentProfile)
        return;
    juce::Reverb::Parameters parameters;
    parameters.roomSize = styleRoomSize.load(std::memory_order_relaxed);
    parameters.damping = styleDamping.load(std::memory_order_relaxed);
    parameters.wetLevel = juce::jlimit(0.0f, 0.48f, mix * settings.reverbScale);
    parameters.dryLevel = 1.0f - parameters.wetLevel * 0.32f;
    parameters.width = styleWidth.load(std::memory_order_relaxed);
    instrumentReverb.setParameters(parameters);
    lastInstrumentReverbMix = mix;
    lastInstrumentProfile = profile;
}

void MasterOutputService::processInstrument(float* const* outputs, int channels, int samples) noexcept
{
    if (outputs == nullptr || channels <= 0 || samples <= 0) return;
    const auto smart = smartOptimisation.load(std::memory_order_relaxed);
    const auto settings = currentProfileSettings();
    const auto requestedTone = eqTone.load(std::memory_order_relaxed);
    const auto tone = juce::jlimit(-1.0f, 1.0f, requestedTone + (smart ? settings.toneBias : 0.0f));
    const auto rate = static_cast<float>(sampleRate.load(std::memory_order_relaxed));
    const auto envelopeAttack = 1.0f - std::exp(-1.0f / (0.012f * rate));
    const auto envelopeRelease = 1.0f - std::exp(-1.0f / (0.24f * rate));
    const auto gainRelease = 1.0f - std::exp(-1.0f / (0.18f * rate));
    const auto trimSpeed = 1.0f - std::exp(-1.0f / (2.8f * rate));
    const auto duckAttack = 1.0f - std::exp(-1.0f / (0.055f * rate));
    const auto duckRelease = 1.0f - std::exp(-1.0f / (0.42f * rate));

    for (int sample = 0; sample < samples; ++sample)
    {
        float linkedPeak = 0.0f;
        for (int channel = 0; channel < channels; ++channel)
            if (outputs[channel] != nullptr)
                linkedPeak = juce::jmax(linkedPeak, std::abs(outputs[channel][sample]));
        instrumentEnvelope += (linkedPeak - instrumentEnvelope)
                            * (linkedPeak > instrumentEnvelope ? envelopeAttack : envelopeRelease);

        auto desiredCompressorGain = 1.0f;
        const auto threshold = styleCompressionThreshold.load(std::memory_order_relaxed);
        const auto ratio = styleCompressionRatio.load(std::memory_order_relaxed);
        if (smart && instrumentEnvelope > threshold)
        {
            const auto compressed = threshold + (instrumentEnvelope - threshold) / ratio;
            desiredCompressorGain = compressed / juce::jmax(0.0001f, instrumentEnvelope);
        }
        compressorGain += (desiredCompressorGain - compressorGain)
                        * (desiredCompressorGain < compressorGain ? 0.18f : gainRelease);

        if (smart && instrumentEnvelope > 0.04f)
        {
            const auto desiredTrim = juce::jlimit(0.84f, 1.18f, 0.32f / instrumentEnvelope);
            automaticTrim += (desiredTrim - automaticTrim) * trimSpeed;
        }
        else if (! smart)
            automaticTrim += (1.0f - automaticTrim) * trimSpeed;

        const auto activity = juce::jlimit(0.0f, 1.0f, (instrumentEnvelope - 0.025f) / 0.22f);
        const auto desiredDuck = smart ? (1.0f - activity * 0.16f) : 1.0f;
        duckGainState += (desiredDuck - duckGainState)
                       * (desiredDuck < duckGainState ? duckAttack : duckRelease);

        for (int channel = 0; channel < channels; ++channel)
        {
            auto* data = outputs[channel];
            if (data == nullptr) continue;
            const auto lane = static_cast<std::size_t>(juce::jmin(channel, 1));
            auto value = data[sample];
            rumbleLowPass[lane] += 0.004f * (value - rumbleLowPass[lane]);
            value -= rumbleLowPass[lane];
            toneLowPass[lane] += 0.075f * (value - toneLowPass[lane]);
            const auto highBand = value - toneLowPass[lane];
            value += tone >= 0.0f ? tone * 0.24f * highBand
                                  : tone * 0.20f * highBand;
            const auto warm = warmth.load(std::memory_order_relaxed);
            value += toneLowPass[lane] * warm * 0.075f;
            const auto harshControl = smart
                ? activity * styleHarshControl.load(std::memory_order_relaxed) : 0.0f;
            value -= highBand * harshControl;
            const auto saturation = styleSaturation.load(std::memory_order_relaxed);
            if (saturation > 0.001f)
            {
                const auto drive = 1.0f + saturation * 4.0f;
                const auto saturated = std::tanh(value * drive) / std::tanh(drive);
                value += (saturated - value) * saturation;
            }
            data[sample] = value * compressorGain * automaticTrim * styleOutputGain.load(std::memory_order_relaxed);
        }
    }
    accompanimentDuckGain.store(duckGainState, std::memory_order_relaxed);

    const auto wetMix = reverbMix.load(std::memory_order_relaxed);
    updateInstrumentReverb(wetMix, settings);
    if (channels > 1 && outputs[0] != nullptr && outputs[1] != nullptr)
        instrumentReverb.processStereo(outputs[0], outputs[1], samples);
    else if (outputs[0] != nullptr)
        instrumentReverb.processMono(outputs[0], samples);
}

void MasterOutputService::processMaster(float* const* outputs, int channels, int samples) noexcept
{
    if (outputs == nullptr || channels <= 0 || samples <= 0) return;
    if (smartOptimisation.load(std::memory_order_relaxed))
    {
        if (channels > 1 && outputs[0] != nullptr && outputs[1] != nullptr)
            glueReverb.processStereo(outputs[0], outputs[1], samples);
        else if (outputs[0] != nullptr)
            glueReverb.processMono(outputs[0], samples);
    }

    const auto currentGain = gain.load(std::memory_order_relaxed);
    const auto ceiling = limiterCeiling.load(std::memory_order_relaxed);
    const auto release = 1.0f - std::exp(-1.0f / (0.12f * static_cast<float>(sampleRate.load())));
    for (int sample = 0; sample < samples; ++sample)
    {
        float linkedPeak = 0.0f;
        for (int channel = 0; channel < channels; ++channel)
            if (outputs[channel] != nullptr)
                linkedPeak = juce::jmax(linkedPeak, std::abs(outputs[channel][sample] * currentGain));
        const auto desired = linkedPeak > ceiling ? ceiling / linkedPeak : 1.0f;
        limiterGain = desired < limiterGain ? desired : limiterGain + (1.0f - limiterGain) * release;
        for (int channel = 0; channel < channels; ++channel)
            if (outputs[channel] != nullptr)
                outputs[channel][sample] *= currentGain * limiterGain;
    }
    updateSpectrumAndPeaks(outputs, channels, samples);
}

void MasterOutputService::updateSpectrumAndPeaks(float* const* outputs, int channels, int samples) noexcept
{
    float peaks[2] {};
    for (int channel = 0; channel < channels; ++channel)
        if (outputs[channel] != nullptr)
            for (int sample = 0; sample < samples; ++sample)
                peaks[juce::jmin(channel, 1)] = juce::jmax(peaks[juce::jmin(channel, 1)], std::abs(outputs[channel][sample]));
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
