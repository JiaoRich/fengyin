#pragma once

#include <juce_video/juce_video.h>
#include "AccompanimentAudioService.h"
#include "VideoAudioExtractor.h"

namespace fengyin
{
class VideoPlayerPanel final : public juce::Component, private juce::Timer
{
public:
    VideoPlayerPanel();
    ~VideoPlayerPanel() override;
    void setAccompanimentService(AccompanimentAudioService* service) noexcept { accompaniment = service; }
    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void chooseVideo();
    void togglePlayback();
    void loadFile(const juce::File& file);
    static juce::String formatTime(double seconds);
    static juce::String utf8(const char* text);

    juce::VideoComponent video { false };
    juce::TextButton chooseButton;
    juce::TextButton playButton;
    juce::TextButton stopButton;
    juce::Slider position;
    juce::Slider volume;
    juce::Label fileName;
    juce::Label timeLabel;
    std::unique_ptr<juce::FileChooser> chooser;
    bool userDraggingPosition = false;
    int sustainedDriftChecks = 0;
    double lastHardSyncAtMs = 0.0;
    AccompanimentAudioService* accompaniment = nullptr;
    VideoAudioExtractor extractor;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VideoPlayerPanel)
};
}
