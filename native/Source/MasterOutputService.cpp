#include "MasterOutputService.h"
#include "../../audio-engine/common/PerformanceMixPolicy.h"

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
    // Match the persisted package range; do not silently discard tuner levels.
    styleOutputGain.store(std::isfinite(settings.outputGain) ? juce::jlimit(0.0f, 2.0f, settings.outputGain) : 1.0f);
    bassTone.store(juce::jlimit(-1.0f, 1.0f, settings.bass));
    airAmount.store(std::isfinite(settings.air) ? juce::jlimit(0.0f, 1.0f, settings.air) : 0.0f);
    reverbParametersDirty.store(true, std::memory_order_release);
}

ToneStyleSettings MasterOutputService::getToneStyle() const noexcept
{
    return { eqTone.load(), warmth.load(), reverbMix.load(), styleCompressionThreshold.load(),
             styleCompressionRatio.load(), styleSaturation.load(), styleHarshControl.load(),
             styleRoomSize.load(), styleDamping.load(), styleWidth.load(), styleOutputGain.load(), bassTone.load(), airAmount.load() };
}

void MasterOutputService::setSampleRate(double value) noexcept
{
    const auto validRate = value > 0.0 ? value : 48000.0;
    sampleRate.store(validRate, std::memory_order_relaxed);
    // Device start is not audible yet: initialise ramps at their current targets.
    // Later UI changes remain smoothed in the audio callback.
    smoothedBassGain = juce::Decibels::decibelsToGain(bassTone.load(std::memory_order_relaxed) * 12.0f);
    smoothedOutputGain = styleOutputGain.load(std::memory_order_relaxed);
    instrumentReverb.setSampleRate(validRate);
    airProcessor.prepare(validRate);
    instrumentReverb.reset();
    wetPreDelay.prepare(validRate);
    wetPreDelay.setDelay(reverbPreDelayMs.load());
    lastInstrumentReverbMix = -1.0f;
}

MasterOutputService::ProfileSettings MasterOutputService::currentProfileSettings() const noexcept
{
    // Instrument identity must not add a second, hidden mix preset.
    return {};
}

void MasterOutputService::updateInstrumentReverb(float mix, const ProfileSettings& settings) noexcept
{
    const auto profile = instrumentProfile.load(std::memory_order_relaxed);
    if (! reverbParametersDirty.exchange(false, std::memory_order_acq_rel)
        && std::abs(mix - lastInstrumentReverbMix) < 0.001f && profile == lastInstrumentProfile)
        return;
    juce::Reverb::Parameters parameters;
    parameters.roomSize = styleRoomSize.load(std::memory_order_relaxed);
    parameters.damping = styleDamping.load(std::memory_order_relaxed);
    parameters.wetLevel = juce::jlimit(0.0f, 0.48f, mix * settings.reverbScale);
    // JUCE multiplies dryLevel by two internally; 0.5 means unity, not 1.0.
    parameters.dryLevel = 0.0f; // dry is mixed immediately, outside the wet-only delay.
    parameters.width = styleWidth.load(std::memory_order_relaxed);
    instrumentReverb.setParameters(parameters);
    lastInstrumentReverbMix = mix;
    lastInstrumentProfile = profile;
}

void MasterOutputService::processInstrument(float* const* outputs, int channels, int samples) noexcept
{
    if (outputs == nullptr || channels <= 0 || samples <= 0) return;
    const auto settings = currentProfileSettings();
    const auto requestedTone = eqTone.load(std::memory_order_relaxed);
    const auto tone = juce::jlimit(-1.0f, 1.0f, requestedTone);
    const auto rate = static_cast<float>(sampleRate.load(std::memory_order_relaxed));
    const auto envelopeAttack = 1.0f - std::exp(-1.0f / (0.012f * rate));
    const auto envelopeRelease = 1.0f - std::exp(-1.0f / (0.24f * rate));
    const auto gainRelease = 1.0f - std::exp(-1.0f / (0.18f * rate));
    // Snapshot UI-controlled atomics once per block. Loading them for every
    // sample unnecessarily lengthens the real-time callback on older laptops.
    const auto threshold = styleCompressionThreshold.load(std::memory_order_relaxed);
    const auto ratio = styleCompressionRatio.load(std::memory_order_relaxed);
    const auto warm = warmth.load(std::memory_order_relaxed);
    const auto harshStyle = styleHarshControl.load(std::memory_order_relaxed);
    const auto saturation = styleSaturation.load(std::memory_order_relaxed);
    const auto styleGain = styleOutputGain.load(std::memory_order_relaxed);
    const auto bassGain = juce::Decibels::decibelsToGain(bassTone.load(std::memory_order_relaxed) * 12.0f);
    const auto bassCoefficient = 1.0f - std::exp(-2.0f * juce::MathConstants<float>::pi * 180.0f / rate);
    const auto smoothing = 1.0f - std::exp(-1.0f / (0.01f * rate));
    const auto saturationDrive = 1.0f + saturation * 4.0f;
    const auto saturationNormaliser = saturation > 0.001f ? 1.0f / std::tanh(saturationDrive) : 1.0f;
    const auto saturationMix = juce::jlimit(0.0f, 0.85f, 0.18f + saturation * 1.9f);

    for (int sample = 0; sample < samples; ++sample)
    {
        smoothedBassGain += (bassGain - smoothedBassGain) * smoothing;
        smoothedOutputGain += (styleGain - smoothedOutputGain) * smoothing;
        float linkedPeak = 0.0f;
        for (int channel = 0; channel < channels; ++channel)
            if (outputs[channel] != nullptr)
                linkedPeak = juce::jmax(linkedPeak, std::abs(outputs[channel][sample]));
        instrumentEnvelope += (linkedPeak - instrumentEnvelope)
                            * (linkedPeak > instrumentEnvelope ? envelopeAttack : envelopeRelease);

        auto desiredCompressorGain = 1.0f;
        if (instrumentEnvelope > threshold)
        {
            const auto compressed = threshold + (instrumentEnvelope - threshold) / ratio;
            desiredCompressorGain = compressed / juce::jmax(0.0001f, instrumentEnvelope);
        }
        compressorGain += (desiredCompressorGain - compressorGain)
                        * (desiredCompressorGain < compressorGain ? 0.18f : gainRelease);


        const auto activity = juce::jlimit(0.0f, 1.0f, (instrumentEnvelope - 0.025f) / 0.22f);
        for (int channel = 0; channel < channels; ++channel)
        {
            auto* data = outputs[channel];
            if (data == nullptr) continue;
            const auto lane = static_cast<std::size_t>(juce::jmin(channel, 1));
            auto value = data[sample];
            bassLowPass[lane] += bassCoefficient * (value - bassLowPass[lane]);
            value += bassLowPass[lane] * (smoothedBassGain - 1.0f);
            toneLowPass[lane] += 0.075f * (value - toneLowPass[lane]);
            const auto highBand = value - toneLowPass[lane];
            value += tone >= 0.0f ? tone * 0.52f * highBand
                                  : tone * 0.38f * highBand;
            value += toneLowPass[lane] * warm * 0.14f;
            const auto harshControl = activity * harshStyle;
            value -= highBand * harshControl;
            if (saturation > 0.001f)
            {
                const auto saturated = std::tanh(value * saturationDrive) * saturationNormaliser;
                value += (saturated - value) * saturationMix;
            }
            data[sample] = value * compressorGain * smoothedOutputGain;
        }
    }
    airProcessor.process(outputs, channels, samples, airAmount.load(std::memory_order_relaxed));
    const auto wetMix = reverbMix.load(std::memory_order_relaxed);
    updateInstrumentReverb(wetMix, settings);
    if (wetMix > 0.0f)
    {
        const auto dryScale=1.0f-juce::jlimit(0.0f,.48f,wetMix*settings.reverbScale)*.32f;
        wetPreDelay.setDelay(reverbPreDelayMs.load());
        for(int pos=0;pos<samples;pos+=512) {
            const int n=juce::jmin(512,samples-pos);const bool stereo=channels>1&&outputs[1]!=nullptr;
            for(int c=0;c<(stereo?2:1);++c)if(outputs[c])std::copy_n(outputs[c]+pos,n,wetScratch[c].data());
            if(stereo)instrumentReverb.processStereo(wetScratch[0].data(),wetScratch[1].data(),n);
            else instrumentReverb.processMono(wetScratch[0].data(),n);
            wetPreDelay.process(wetScratch[0].data(),stereo?wetScratch[1].data():nullptr,n);
            for(int c=0;c<(stereo?2:1);++c)if(outputs[c])for(int i=0;i<n;++i)outputs[c][pos+i]=outputs[c][pos+i]*dryScale+wetScratch[c][i];
        }
    }
    else {instrumentReverb.reset();wetPreDelay.reset();}

    // The visible performance fader owns only the instrument bus. No limiter,
    // fixed makeup gain or master gain is applied to the accompaniment.
    const auto performanceVolume = gain.load(std::memory_order_relaxed);
    float blockPeak = 0.0f;
    for (int channel = 0; channel < channels; ++channel)
        if (outputs[channel] != nullptr)
            juce::FloatVectorOperations::multiply(outputs[channel], performanceVolume, samples);
    for (int sample = 0; sample < samples; ++sample)
    {
        float linkedPeak = 0.0f;
        for (int channel = 0; channel < channels; ++channel)
            if (outputs[channel] != nullptr)
                linkedPeak = juce::jmax(linkedPeak, std::abs(outputs[channel][sample]));
        blockPeak = juce::jmax(blockPeak, linkedPeak);
    }
    instrumentPeak.store(blockPeak, std::memory_order_relaxed);
}

void MasterOutputService::processMaster(float* const* outputs, int channels, int samples) noexcept
{
    if (outputs == nullptr || channels <= 0 || samples <= 0) return;
    constexpr auto digitalCeiling = 1.0f;
    uint64_t overloaded = 0;
    for (int sample = 0; sample < samples; ++sample)
    {
        float linkedPeak = 0.0f;
        for (int channel = 0; channel < channels; ++channel)
            if (outputs[channel] != nullptr)
                linkedPeak = juce::jmax(linkedPeak, std::abs(outputs[channel][sample]));
        // Meter overloads without changing samples. Final PCM conversion may
        // clip values outside its representable range; that is not gain control.
        if (linkedPeak > digitalCeiling) ++overloaded;
    }
    updateSpectrumAndPeaks(outputs, channels, samples);
    if (overloaded != 0) overloadSamples.fetch_add(overloaded, std::memory_order_relaxed);
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
