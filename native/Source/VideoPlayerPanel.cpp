#include "VideoPlayerPanel.h"

#include <cmath>

namespace fengyin
{
VideoPlayerPanel::VideoPlayerPanel()
{
    addAndMakeVisible(video);
    chooseButton.setButtonText(utf8("选择视频"));
    chooseButton.onClick = [this] { chooseVideo(); };
    addAndMakeVisible(chooseButton);
    playButton.setButtonText(utf8("播放"));
    playButton.onClick = [this] { togglePlayback(); };
    addAndMakeVisible(playButton);
    stopButton.setButtonText(utf8("停止"));
    stopButton.onClick = [this]
    {
        video.stop();
        video.setPlayPosition(0.0);
        if (accompaniment != nullptr) accompaniment->stopAndRewind();
    };
    addAndMakeVisible(stopButton);

    position.setSliderStyle(juce::Slider::LinearHorizontal);
    position.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    position.setRange(0.0, 1.0, 0.001);
    position.onDragStart = [this] { userDraggingPosition = true; };
    position.onDragEnd = [this]
    {
        if (video.isVideoOpen())
        {
            video.setPlayPosition(position.getValue() * video.getVideoDuration());
            if (accompaniment != nullptr) accompaniment->setPosition(video.getPlayPosition());
        }
        userDraggingPosition = false;
    };
    addAndMakeVisible(position);

    volume.setSliderStyle(juce::Slider::LinearHorizontal);
    volume.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    volume.setRange(0.0, 1.0, 0.01);
    volume.setValue(0.72, juce::dontSendNotification);
    volume.onValueChange = [this]
    {
        if (accompaniment != nullptr && accompaniment->isReady()) accompaniment->setVolume(static_cast<float>(volume.getValue()));
        else video.setAudioVolume(static_cast<float>(volume.getValue()));
    };
    addAndMakeVisible(volume);

    fileName.setText(utf8("尚未选择动态谱视频"), juce::dontSendNotification);
    fileName.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(fileName);
    timeLabel.setText("00:00 / 00:00", juce::dontSendNotification);
    timeLabel.setJustificationType(juce::Justification::centredRight);
    timeLabel.setColour(juce::Label::textColourId, juce::Colour(0xff8fa7bd));
    addAndMakeVisible(timeLabel);

    video.onPlaybackStarted = [this]
    {
        playButton.setButtonText(utf8("暂停"));
        if (accompaniment != nullptr && accompaniment->isReady())
        { accompaniment->setPosition(video.getPlayPosition()); accompaniment->play(); }
    };
    video.onPlaybackStopped = [this]
    {
        playButton.setButtonText(utf8("播放"));
        if (accompaniment != nullptr) accompaniment->pause();
    };
    video.onErrorOccurred = [this](const juce::String& error)
    {
        const auto message = utf8("视频播放失败。请优先使用 MP4（H.264 视频 + AAC 音频）。\n\n系统返回：") + error;
        fileName.setText(utf8("视频播放失败 · 点击“选择视频”重试"), juce::dontSendNotification);
        juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,
                                               utf8("视频无法播放"), message);
    };
    startTimerHz(10);
}

VideoPlayerPanel::~VideoPlayerPanel()
{
    stopTimer();
    chooser.reset();
    video.closeVideo();
}

void VideoPlayerPanel::paint(juce::Graphics& g)
{
    g.setColour(juce::Colour(0xff07101d));
    g.fillRoundedRectangle(getLocalBounds().toFloat(), 14.0f);
    if (! video.isVideoOpen())
    {
        g.setColour(juce::Colour(0xff8fa7bd));
        g.setFont(juce::FontOptions(17.0f));
        g.drawText(utf8("选择带动态谱或歌词的 MP4 视频"),
                   getLocalBounds().withTrimmedBottom(64), juce::Justification::centred);
    }
}

void VideoPlayerPanel::resized()
{
    auto area = getLocalBounds().reduced(12);
    auto titleRow = area.removeFromTop(34);
    chooseButton.setBounds(titleRow.removeFromRight(100));
    fileName.setBounds(titleRow);
    auto controls = area.removeFromBottom(44);
    playButton.setBounds(controls.removeFromLeft(66).reduced(3, 6));
    stopButton.setBounds(controls.removeFromLeft(58).reduced(3, 6));
    timeLabel.setBounds(controls.removeFromRight(112));
    volume.setBounds(controls.removeFromRight(88));
    controls.removeFromRight(6);
    position.setBounds(controls.reduced(4, 7));
    video.setBounds(area.reduced(0, 6));
}

void VideoPlayerPanel::timerCallback()
{
    const auto duration = video.getVideoDuration();
    const auto current = video.getPlayPosition();
    if (video.isPlaying() && accompaniment != nullptr && accompaniment->isReady()
        && std::abs(accompaniment->getPosition() - current) > 0.25)
        accompaniment->setPosition(current);
    if (! userDraggingPosition && duration > 0.0)
        position.setValue(current / duration, juce::dontSendNotification);
    timeLabel.setText(formatTime(current) + " / " + formatTime(duration), juce::dontSendNotification);
}

void VideoPlayerPanel::chooseVideo()
{
    chooser = std::make_unique<juce::FileChooser>(utf8("选择动态谱视频"), juce::File(),
                                                  "*.mp4;*.mov;*.m4v;*.avi;*.wmv;*.mpeg;*.mpg");
    const auto flags = juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles;
    chooser->launchAsync(flags, [safe = juce::Component::SafePointer<VideoPlayerPanel>(this)](const juce::FileChooser& dialog)
    {
        if (safe != nullptr && dialog.getResult().existsAsFile())
            safe->loadFile(dialog.getResult());
    });
}

void VideoPlayerPanel::togglePlayback()
{
    if (! video.isVideoOpen())
    {
        chooseVideo();
        return;
    }
    if (video.isPlaying())
        video.stop();
    else
        video.play();
}

void VideoPlayerPanel::loadFile(const juce::File& file)
{
    fileName.setText(utf8("正在载入：") + file.getFileName(), juce::dontSendNotification);
    video.loadAsync(juce::URL(file),
                    [safe = juce::Component::SafePointer<VideoPlayerPanel>(this), file](const juce::URL&,
                                                                                       juce::Result result)
                    {
                        if (safe == nullptr)
                            return;
                        if (result.wasOk())
                        {
                            const auto unified = safe->accompaniment != nullptr && safe->accompaniment->loadForVideo(file);
                            safe->fileName.setText(file.getFileName() + (unified ? utf8(" · 伴奏可录制") : utf8(" · 正在准备伴奏…")),
                                                   juce::dontSendNotification);
                            safe->video.setAudioVolume(unified ? 0.0f : static_cast<float>(safe->volume.getValue()));
                            if (unified) safe->accompaniment->setVolume(static_cast<float>(safe->volume.getValue()));
                            if (! unified)
                            {
                                safe->extractor.extractAsync(file,
                                    [safe, file](bool success, const juce::File& extracted, const juce::String&)
                                    {
                                        if (safe == nullptr || safe->accompaniment == nullptr) return;
                                        const auto loaded = success && safe->accompaniment->loadAudioFile(extracted);
                                        safe->fileName.setText(file.getFileName() + (loaded ? utf8(" · 伴奏可录制")
                                                                                    : utf8(" · 伴奏仅播放")),
                                                               juce::dontSendNotification);
                                        if (loaded)
                                        {
                                            safe->video.setAudioVolume(0.0f);
                                            safe->accompaniment->setVolume(static_cast<float>(safe->volume.getValue()));
                                            safe->accompaniment->setPosition(safe->video.getPlayPosition());
                                            if (safe->video.isPlaying()) safe->accompaniment->play();
                                        }
                                    });
                            }
                        }
                        else
                        {
                            const auto message = utf8("无法载入该视频。请优先使用 MP4（H.264 视频 + AAC 音频）。\n\n文件：")
                                               + file.getFullPathName() + utf8("\n\n系统返回：") + result.getErrorMessage();
                            safe->fileName.setText(utf8("无法载入视频 · 已显示处理建议"), juce::dontSendNotification);
                            juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,
                                                                   utf8("视频无法载入"), message);
                        }
                        safe->repaint();
                    });
}

juce::String VideoPlayerPanel::formatTime(double seconds)
{
    if (! std::isfinite(seconds) || seconds < 0.0)
        seconds = 0.0;
    const auto total = static_cast<int>(seconds);
    return juce::String(total / 60).paddedLeft('0', 2) + ":"
         + juce::String(total % 60).paddedLeft('0', 2);
}

juce::String VideoPlayerPanel::utf8(const char* text)
{
    return juce::String::fromUTF8(text);
}
}
