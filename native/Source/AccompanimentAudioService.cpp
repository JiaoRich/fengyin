#include "AccompanimentAudioService.h"
namespace fengyin
{
AccompanimentAudioService::AccompanimentAudioService() { formats.registerBasicFormats(); readAheadThread.startThread(); }
AccompanimentAudioService::~AccompanimentAudioService() { unload(); readAheadThread.stopThread(2000); }
juce::File AccompanimentAudioService::findDecodableFile(const juce::File& videoFile)
{
    if (std::unique_ptr<juce::AudioFormatReader> probe(formats.createReaderFor(videoFile)); probe != nullptr)
        return videoFile;
    static constexpr const char* extensions[] { ".wav", ".mp3", ".flac", ".aif", ".aiff" };
    for (const auto* extension : extensions)
    {
        const auto candidate = videoFile.withFileExtension(extension);
        if (candidate.existsAsFile())
            if (std::unique_ptr<juce::AudioFormatReader> probe(formats.createReaderFor(candidate)); probe != nullptr)
                return candidate;
    }
    return {};
}
bool AccompanimentAudioService::loadForVideo(const juce::File& videoFile)
{
    const auto audioFile = findDecodableFile(videoFile);
    return audioFile.existsAsFile() && loadAudioFile(audioFile);
}

bool AccompanimentAudioService::loadAudioFile(const juce::File& audioFile)
{
    unload();
    auto reader = std::unique_ptr<juce::AudioFormatReader>(formats.createReaderFor(audioFile));
    if (reader == nullptr) return false;
    const juce::ScopedLock lock(stateLock);
    readerSource = std::make_unique<juce::AudioFormatReaderSource>(reader.release(), true);
    // 约 5 秒的预读余量可隔离视频解码、磁盘瞬时繁忙和界面动画造成的抖动。
    // 旧值只有 32768 个采样，播放高码率视频时很容易被耗尽并让实时音频线程等待。
    constexpr int accompanimentReadAheadSamples = 262144;
    transport.setSource(readerSource.get(), accompanimentReadAheadSamples, &readAheadThread,
                        readerSource->getAudioFormatReader()->sampleRate);
    loadedFile = audioFile;
    ready.store(true, std::memory_order_release);
    return true;
}
void AccompanimentAudioService::unload()
{
    ready.store(false, std::memory_order_release);
    const juce::ScopedLock lock(stateLock);
    transport.stop();
    transport.setSource(nullptr);
    readerSource.reset();
    loadedFile = juce::File();
}
void AccompanimentAudioService::prepare(double sampleRate, int maximumBlockSize)
{
    transport.prepareToPlay(maximumBlockSize, sampleRate);
    mixBuffer.setSize(2, maximumBlockSize, false, false, true);
}
void AccompanimentAudioService::release() { transport.releaseResources(); }
void AccompanimentAudioService::play() { if (isReady()) transport.start(); }
void AccompanimentAudioService::pause() { transport.stop(); }
void AccompanimentAudioService::stopAndRewind() { transport.stop(); transport.setPosition(0.0); }
void AccompanimentAudioService::setPosition(double seconds) { if (isReady()) transport.setPosition(juce::jmax(0.0, seconds)); }
void AccompanimentAudioService::setVolume(float volume) { transport.setGain(juce::jlimit(0.0f, 1.0f, volume)); }
void AccompanimentAudioService::mixInto(float* const* outputs, int outputChannels, int sampleCount,
                                        float gainMultiplier) noexcept
{
    if (! isReady() || outputs == nullptr || outputChannels <= 0 || sampleCount <= 0 || mixBuffer.getNumSamples() < sampleCount) return;
    mixBuffer.clear();
    juce::AudioSourceChannelInfo info(&mixBuffer, 0, sampleCount);
    transport.getNextAudioBlock(info);
    const auto mixGain = juce::jlimit(0.0f, 1.0f, gainMultiplier);
    for (int channel = 0; channel < outputChannels; ++channel)
        if (outputs[channel] != nullptr)
            juce::FloatVectorOperations::addWithMultiply(outputs[channel],
                mixBuffer.getReadPointer(juce::jmin(channel, 1)), mixGain, sampleCount);
}
double AccompanimentAudioService::getPosition() const { return transport.getCurrentPosition(); }
juce::File AccompanimentAudioService::getAudioFile() const { const juce::ScopedLock lock(stateLock); return loadedFile; }
}
