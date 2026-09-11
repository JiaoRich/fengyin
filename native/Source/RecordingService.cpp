#include "RecordingService.h"

namespace fengyin
{
RecordingService::RecordingService() { writerThread.startThread(); }
RecordingService::~RecordingService() { stop(); writerThread.stopThread(2000); }

bool RecordingService::start(double sampleRate, int channelCount)
{
    auto folder = getRecordingsFolder();
    if (! folder.createDirectory())
    {
        const juce::ScopedLock lock(stateLock);
        lastError = juce::String::fromUTF8("无法创建“风吟录音”文件夹");
        return false;
    }
    return startToFile(folder.getNonexistentChildFile("风吟-" + juce::Time::getCurrentTime().formatted("%Y%m%d-%H%M%S"),
                                                      ".wav", false), sampleRate, channelCount);
}

bool RecordingService::startToFile(const juce::File& destination, double sampleRate, int channelCount)
{
    stop();
    const juce::ScopedLock lock(stateLock);
    lastError.clear();
    if (sampleRate <= 0.0 || channelCount <= 0)
    {
        lastError = juce::String::fromUTF8("音频设备尚未准备好");
        return false;
    }
    lastFile = destination;
    lastFile.getParentDirectory().createDirectory();
    std::unique_ptr<juce::OutputStream> stream(lastFile.createOutputStream());
    if (stream == nullptr)
    {
        lastError = juce::String::fromUTF8("无法创建录音文件，请检查磁盘空间");
        return false;
    }
    juce::WavAudioFormat wav;
    const auto options = juce::AudioFormatWriterOptions().withSampleRate(sampleRate)
                                                       .withNumChannels(juce::jmin(channelCount, 2))
                                                       .withBitsPerSample(24);
    auto writer = wav.createWriterFor(stream, options);
    if (writer == nullptr)
    {
        lastError = juce::String::fromUTF8("无法初始化 WAV 录音器");
        return false;
    }
    threadedWriter = std::make_unique<juce::AudioFormatWriter::ThreadedWriter>(writer.release(), writerThread, 65536);
    recordingSampleRate = sampleRate;
    samplesWritten.store(0, std::memory_order_release);
    activeWriter.store(threadedWriter.get(), std::memory_order_release);
    return true;
}

void RecordingService::stop()
{
    activeWriter.store(nullptr, std::memory_order_release);
    while (activeCallbacks.load(std::memory_order_acquire) != 0)
        juce::Thread::yield();
    const juce::ScopedLock lock(stateLock);
    threadedWriter.reset();
}

void RecordingService::push(float* const* channels, int channelCount, int sampleCount) noexcept
{
    activeCallbacks.fetch_add(1, std::memory_order_acq_rel);
    auto* writer = activeWriter.load(std::memory_order_acquire);
    if (writer != nullptr && channels != nullptr && channelCount > 0 && sampleCount > 0)
    {
        const float* sources[2] { channels[0], channelCount > 1 ? channels[1] : channels[0] };
        if (sources[0] != nullptr && writer->write(sources, sampleCount))
            samplesWritten.fetch_add(sampleCount, std::memory_order_relaxed);
    }
    activeCallbacks.fetch_sub(1, std::memory_order_release);
}

double RecordingService::getElapsedSeconds() const noexcept
{
    return recordingSampleRate > 0.0 ? static_cast<double>(samplesWritten.load(std::memory_order_relaxed)) / recordingSampleRate : 0.0;
}

juce::File RecordingService::getLastFile() const { const juce::ScopedLock lock(stateLock); return lastFile; }
juce::String RecordingService::getLastError() const { const juce::ScopedLock lock(stateLock); return lastError; }

juce::File RecordingService::getRecordingsFolder()
{
    return juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
        .getChildFile(juce::String::fromUTF8("风吟录音"));
}

juce::Array<juce::File> RecordingService::getRecordings()
{
    juce::Array<juce::File> files;
    getRecordingsFolder().findChildFiles(files, juce::File::findFiles, false, "*.wav");
    std::sort(files.begin(), files.end(), [](const auto& a, const auto& b)
    { return a.getLastModificationTime() > b.getLastModificationTime(); });
    return files;
}
}
