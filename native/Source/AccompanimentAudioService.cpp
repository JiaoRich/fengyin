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
    transport.setSource(readerSource.get(), 32768, &readAheadThread, readerSource->getAudioFormatReader()->sampleRate);
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
