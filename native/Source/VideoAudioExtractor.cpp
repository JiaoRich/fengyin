#include "VideoAudioExtractor.h"

namespace fengyin
{
class VideoAudioExtractor::ExtractionJob final : public juce::ThreadPoolJob
{
public:
    ExtractionJob(juce::File executableIn, juce::File sourceIn, juce::File destinationIn, Completion callbackIn)
        : ThreadPoolJob("Extract video audio"), executable(std::move(executableIn)), source(std::move(sourceIn)),
          destination(std::move(destinationIn)), callback(std::move(callbackIn)) {}

    JobStatus runJob() override
    {
        if (destination.existsAsFile() && destination.getSize() > 44)
        {
            finish(true, {});
            return jobHasFinished;
        }
        destination.getParentDirectory().createDirectory();
        const auto temporary = destination.getSiblingFile(destination.getFileNameWithoutExtension() + ".part.wav");
        temporary.deleteFile();
        juce::StringArray arguments { executable.getFullPathName(), "-hide_banner", "-loglevel", "error", "-y",
                                      "-i", source.getFullPathName(), "-vn", "-ac", "2", "-ar", "48000",
                                      "-c:a", "pcm_s24le", temporary.getFullPathName() };
        juce::ChildProcess process;
        if (! process.start(arguments, juce::ChildProcess::wantStdErr))
        {
            finish(false, juce::String::fromUTF8("无法启动音轨提取组件"));
            return jobHasFinished;
        }
        while (process.isRunning() && ! shouldExit())
            process.waitForProcessToFinish(200);
        if (shouldExit())
        {
            process.kill();
            temporary.deleteFile();
            return jobHasFinished;
        }
        const auto output = process.readAllProcessOutput().trim();
        const auto success = process.getExitCode() == 0 && temporary.getSize() > 44 && temporary.moveFileTo(destination);
        if (! success) temporary.deleteFile();
        finish(success, success ? juce::String() : (output.isNotEmpty() ? output : juce::String::fromUTF8("视频中没有可读取的音轨")));
        return jobHasFinished;
    }
private:
    void finish(bool success, const juce::String& message)
    {
        auto callbackToPost = std::move(callback);
        const auto file = destination;
        juce::MessageManager::callAsync([postedCallback = std::move(callbackToPost), success, file, message]() mutable
        { if (postedCallback) postedCallback(success, file, message); });
    }
    juce::File executable, source, destination;
    Completion callback;
};

VideoAudioExtractor::VideoAudioExtractor() = default;
VideoAudioExtractor::~VideoAudioExtractor() { cancel(); }
void VideoAudioExtractor::cancel() { pool.removeAllJobs(true, 5000); }

juce::File VideoAudioExtractor::getExecutable() const
{
    const auto app = juce::File::getSpecialLocation(juce::File::currentApplicationFile);
   #if JUCE_WINDOWS
    const juce::File candidates[] { app.getParentDirectory().getChildFile("ffmpeg.exe"),
                                    app.getParentDirectory().getChildFile("tools").getChildFile("ffmpeg.exe"),
                                    app.getParentDirectory().getChildFile("tools").getChildFile("ffmpeg").getChildFile("ffmpeg.exe") };
   #else
    const juce::File candidates[] { app.getParentDirectory().getChildFile("ffmpeg"),
                                    juce::File("/opt/homebrew/bin/ffmpeg"), juce::File("/usr/local/bin/ffmpeg") };
   #endif
    for (const auto& candidate : candidates)
        if (candidate.existsAsFile()) return candidate;
    return {};
}
bool VideoAudioExtractor::isExtractorAvailable() const { return getExecutable().existsAsFile(); }

juce::File VideoAudioExtractor::cacheFileFor(const juce::File& video, const juce::File& cacheDirectory)
{
    const auto identity = video.getFullPathName() + "|" + juce::String(video.getSize()) + "|"
                        + juce::String(video.getLastModificationTime().toMilliseconds());
    const auto hash = juce::SHA256(identity.toRawUTF8(), static_cast<size_t>(identity.getNumBytesAsUTF8())).toHexString().substring(0, 24);
    return cacheDirectory.getChildFile(hash + ".wav");
}

void VideoAudioExtractor::extractAsync(const juce::File& video, Completion completion)
{
    cancel();
    const auto executable = getExecutable();
    if (! executable.existsAsFile())
    {
        juce::MessageManager::callAsync([callbackToRun = std::move(completion)]() mutable
        { if (callbackToRun) callbackToRun(false, juce::File(), juce::String::fromUTF8("尚未安装音轨提取组件")); });
        return;
    }
    const auto cache = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                           .getChildFile("FengYin").getChildFile("audio-cache");
    pool.addJob(new ExtractionJob(executable, video, cacheFileFor(video, cache), std::move(completion)), true);
}
}
