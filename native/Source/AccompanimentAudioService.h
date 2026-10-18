#pragma once
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_audio_devices/juce_audio_devices.h>
#include <memory>
namespace fengyin
{
class AccompanimentAudioService
{
public:
    AccompanimentAudioService();
    ~AccompanimentAudioService();
    bool loadForVideo(const juce::File& videoFile);
    bool loadAudioFile(const juce::File& audioFile);
    void unload();
    void prepare(double outputSampleRate, int maximumBlockSize);
    void release();
    void play();
    void pause();
    void stopAndRewind();
    void setPosition(double seconds);
    void setVolume(float volume);
    void mixInto(float* const* outputs, int outputChannels, int sampleCount) noexcept;
    [[nodiscard]] bool isReady() const noexcept { return ready.load(std::memory_order_acquire); }
    [[nodiscard]] bool isPlaying() const noexcept { return transport.isPlaying(); }
    [[nodiscard]] double getPosition() const;
    [[nodiscard]] bool hasFinished() const noexcept { return transport.hasStreamFinished(); }
    [[nodiscard]] juce::File getAudioFile() const;
    [[nodiscard]] uint64_t getReadMissCount() const noexcept { return readMissCount.load(std::memory_order_relaxed); }
private:
    juce::File findDecodableFile(const juce::File& videoFile);
    juce::AudioFormatManager formats;
    juce::TimeSliceThread readAheadThread { "FengYin accompaniment reader" };
    std::unique_ptr<juce::AudioFormatReaderSource> readerSource;
    juce::AudioTransportSource transport;
    juce::AudioBuffer<float> mixBuffer;
    juce::File loadedFile;
    std::atomic<bool> ready { false };
    std::atomic<uint64_t> readMissCount {0};
    juce::CriticalSection stateLock;
};
}
