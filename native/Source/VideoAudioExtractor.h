#pragma once
#include <juce_cryptography/juce_cryptography.h>
#include <juce_events/juce_events.h>
#include <functional>

namespace fengyin
{
class VideoAudioExtractor
{
public:
    using Completion = std::function<void(bool, const juce::File&, const juce::String&)>;
    VideoAudioExtractor();
    ~VideoAudioExtractor();
    void extractAsync(const juce::File& video, Completion completion);
    void cancel();
    [[nodiscard]] bool isExtractorAvailable() const;
    [[nodiscard]] juce::File getExecutable() const;
    static juce::File cacheFileFor(const juce::File& video, const juce::File& cacheDirectory);

private:
    class ExtractionJob;
    juce::ThreadPool pool { 1 };
};
}
