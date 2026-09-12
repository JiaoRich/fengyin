#pragma once
#include <juce_audio_formats/juce_audio_formats.h>
#include <atomic>
#include <memory>

namespace fengyin
{
class RecordingService
{
public:
    RecordingService();
    ~RecordingService();
    bool start(double sampleRate, int channelCount, const juce::String& instrumentChineseName = {});
    [[nodiscard]] static juce::File nextRecordingFile(const juce::File& folder,
                                                      const juce::String& instrumentChineseName,
                                                      const juce::String& date = {});
    bool startToFile(const juce::File& destination, double sampleRate, int channelCount);
    void stop();
    void push(float* const* channels, int channelCount, int sampleCount) noexcept;
    [[nodiscard]] bool isRecording() const noexcept { return activeWriter.load(std::memory_order_acquire) != nullptr; }
    [[nodiscard]] double getElapsedSeconds() const noexcept;
    [[nodiscard]] juce::File getLastFile() const;
    [[nodiscard]] juce::String getLastError() const;
    [[nodiscard]] static juce::File getRecordingsFolder();
    [[nodiscard]] static juce::Array<juce::File> getRecordings();

private:
    juce::TimeSliceThread writerThread { "FengYin WAV writer" };
    std::unique_ptr<juce::AudioFormatWriter::ThreadedWriter> threadedWriter;
    std::atomic<juce::AudioFormatWriter::ThreadedWriter*> activeWriter { nullptr };
    std::atomic<int> activeCallbacks { 0 };
    std::atomic<juce::int64> samplesWritten { 0 };
    double recordingSampleRate = 0.0;
    juce::File lastFile;
    juce::String lastError;
    mutable juce::CriticalSection stateLock;
};
}
