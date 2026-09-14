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
    void mixInto(float* const* outputs, int outputChannels, int sampleCount,
                 float gainMultiplier = 1.0f) noexcept;
    [[nodiscard]] bool isReady() const noexcept { return ready.load(std::memory_order_acquire); }
    [[nodiscard]] double getPosition() const;
    [[nodiscard]] juce::File getAudioFile() const;
private:
    juce::File findDecodableFile(const juce::File& videoFile);
    juce::AudioFormatManager formats;
    juce::TimeSliceThread readAheadThread { "FengYin accompaniment reader" };
    std::unique_ptr<juce::AudioFormatReaderSource> readerSource;
    juce::AudioTransportSource transport;
    juce::AudioBuffer<float> mixBuffer;
    juce::File loadedFile;
    std::atomic<bool> ready { false };
    juce::CriticalSection stateLock;
};
}
