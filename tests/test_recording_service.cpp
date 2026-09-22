#include "RecordingService.h"
#include "AccompanimentAudioService.h"
#include "VideoAudioExtractor.h"
#include "MasterOutputService.h"
#include <cassert>
#include <algorithm>
#include <cmath>
#include <thread>

int main()
{
    fengyin::MasterOutputService master;
    master.setGain(0.5f);
    master.setSampleRate(48000.0);
    master.setSmartOptimisationEnabled(false);
    juce::AudioBuffer<float> analysisAudio(2, 4096);
    for (int i = 0; i < analysisAudio.getNumSamples(); ++i)
        analysisAudio.setSample(0, i, std::sin(static_cast<float>(i) * 0.13f));
    analysisAudio.copyFrom(1, 0, analysisAudio, 0, 0, analysisAudio.getNumSamples());
    master.process(analysisAudio.getArrayOfWritePointers(), 2, analysisAudio.getNumSamples());
    assert(master.getLeftPeak() > 0.49f && master.getLeftPeak() <= 0.5f);
    std::array<float, fengyin::MasterOutputService::spectrumBands> spectrum {};
    assert(master.getSpectrum(spectrum));
    assert(*std::max_element(spectrum.begin(), spectrum.end()) > 0.2f);
    master.setLimiterCeiling(0.7f);
    juce::AudioBuffer<float> limited(2, 64);
    limited.clear();
    limited.setSample(0, 0, 2.0f);
    limited.setSample(1, 0, -2.0f);
    master.process(limited.getArrayOfWritePointers(), 2, limited.getNumSamples());
    assert(limited.getSample(0, 0) <= 1.0f && limited.getSample(1, 0) >= -1.0f);

    master.setGain(1.0f);
    master.setLimiterCeiling(1.0f);
    master.setReverbMix(0.0f);
    master.setSmartOptimisationEnabled(true);
    master.setInstrumentProfile(fengyin::InstrumentMixProfile::saxophone);
    juce::AudioBuffer<float> performance(2, 8192);
    performance.clear();
    for (int i = 0; i < performance.getNumSamples(); ++i)
    {
        performance.setSample(0, i, 0.38f);
        performance.setSample(1, i, 0.38f);
    }
    master.processInstrument(performance.getArrayOfWritePointers(), 2, performance.getNumSamples());
    assert(performance.getMagnitude(0, performance.getNumSamples()) > 0.0f);

    // 伴奏在正常电平下经过总输出时必须保持透明：不随乐器活动降低，
    // 也不再经过总线混响或带释放时间的压限。
    juce::AudioBuffer<float> transparentMaster(2, 256);
    transparentMaster.clear();
    for (int channel = 0; channel < transparentMaster.getNumChannels(); ++channel)
        for (int sample = 0; sample < transparentMaster.getNumSamples(); ++sample)
            transparentMaster.setSample(channel, sample, 0.25f);
    master.processMaster(transparentMaster.getArrayOfWritePointers(), 2, transparentMaster.getNumSamples());
    assert(std::abs(transparentMaster.getSample(0, 0) - 0.25f) < 0.000001f);
    assert(std::abs(transparentMaster.getSample(1, 200) - 0.25f) < 0.000001f);

    const auto folder = juce::File::getSpecialLocation(juce::File::tempDirectory)
                            .getChildFile("fengyin-recording-test-" + juce::Uuid().toString());
    const auto file = folder.getChildFile("test.wav");
    const auto namedFirst = fengyin::RecordingService::nextRecordingFile(folder, juce::String::fromUTF8("高音萨克斯"), "20260911");
    assert(namedFirst.getFileName() == juce::String::fromUTF8("高音萨克斯-20260911-1.wav"));
    assert(namedFirst.create().wasOk());
    const auto namedSecond = fengyin::RecordingService::nextRecordingFile(folder, juce::String::fromUTF8("高音萨克斯"), "20260911");
    assert(namedSecond.getFileName() == juce::String::fromUTF8("高音萨克斯-20260911-2.wav"));
    fengyin::RecordingService recorder;
    assert(recorder.startToFile(file, 48000.0, 2));
    juce::AudioBuffer<float> audio(2, 4800);
    for (int i = 0; i < audio.getNumSamples(); ++i)
    {
        const auto value = 0.2f * std::sin(static_cast<float>(i) * 0.03f);
        audio.setSample(0, i, value);
        audio.setSample(1, i, -value);
    }
    recorder.push(audio.getArrayOfWritePointers(), 2, audio.getNumSamples());
    recorder.stop();
    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    auto reader = std::unique_ptr<juce::AudioFormatReader>(formats.createReaderFor(file));
    assert(reader != nullptr);
    assert(reader->numChannels == 2);
    assert(reader->lengthInSamples == 4800);
    assert(reader->sampleRate == 48000.0);
    const auto video = folder.getChildFile("score.mp4");
    assert(video.create().wasOk());
    const auto cacheA = fengyin::VideoAudioExtractor::cacheFileFor(video, folder.getChildFile("cache"));
    const auto cacheB = fengyin::VideoAudioExtractor::cacheFileFor(video, folder.getChildFile("cache"));
    assert(cacheA == cacheB && cacheA.hasFileExtension("wav"));
    const auto sidecar = folder.getChildFile("score.wav");
    assert(file.copyFileTo(sidecar));
    fengyin::AccompanimentAudioService accompaniment;
    assert(accompaniment.loadForVideo(video));
    accompaniment.prepare(48000.0, 512);
    accompaniment.play();
    juce::AudioBuffer<float> mixed(2, 512);
    for (int tries = 0; tries < 20 && mixed.getMagnitude(0, mixed.getNumSamples()) == 0.0f; ++tries)
    {
        mixed.clear();
        juce::Thread::sleep(5);
        accompaniment.mixInto(mixed.getArrayOfWritePointers(), 2, mixed.getNumSamples());
    }
    assert(mixed.getMagnitude(0, mixed.getNumSamples()) > 0.0f);

    accompaniment.release();
    accompaniment.unload();
    folder.deleteRecursively();
    return 0;
}
