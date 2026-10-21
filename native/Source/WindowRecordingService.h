#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <cstdint>
#include "RecordingStartupGuard.h"

namespace fengyin {
// Separate audio-only and GPU-video helpers; neither runs in the audio graph.
class WindowRecordingService {
public:
    explicit WindowRecordingService(juce::File base = {}) : baseFolder(std::move(base)) {}
    ~WindowRecordingService();
    bool startAudio(bool prepareOnly = false);
    bool startScreen(std::uintptr_t window, bool prepareOnly = false);
    bool beginCapture();
    bool preparing() const noexcept {return running && armed && !finishing;}
    bool prepared() const noexcept {return preparing() && ready;}
    bool wasCancelled() const noexcept {return cancelled;}
    bool hasCleanupWarning() const noexcept {return cleanupWarning;}
    bool hasVideoQualityWarning() const noexcept {return videoQualityWarning;}
    bool hasRecoverableVideo() const {return screen && sessionFolder.getChildFile("alignment.txt").existsAsFile()
        && sessionFolder.getChildFile("audio.wav").existsAsFile() && sessionFolder.getChildFile("video.partial.mp4").existsAsFile();}
    bool generateVideo();
    bool resumeScreenSession(const juce::File& folder);
    bool needsVideoGeneration() const noexcept {return captured;}
    bool isAudioOnly() const noexcept {return !screen;}
    double elapsedSeconds() const noexcept {return running?(juce::Time::getMillisecondCounterHiRes()-startedAt)/1000.0:0.0;}
    void stop();
    void poll();
    bool busy() const noexcept {return running || captured;}
    bool stopping() const noexcept {return finishing;}
    bool recording() const noexcept {return running && !armed && !finishing && ready;}
    juce::String error() const {return lastError;}
    juce::File lastFile() const {return destination;}
    bool takeCompleted() noexcept {const bool v=completed;completed=false;return v;}
private:
    juce::ChildProcess process;
    juce::File destination, stopFile, stateFile, partialFile, sessionFolder, baseFolder;
    juce::File audioFolder() const;
    juce::File videoFolder() const;
    juce::File temporaryFolder() const;
    bool createSession();
    void cleanCompletedSession();
    juce::String lastError;
    RecordingStartupGuard startupGuard;
    bool running=false,finishing=false,ready=false,completed=false;
    bool screen=false,captured=false,muxing=false;
    bool armed=false,cancelled=false,cleanupWarning=false,videoQualityWarning=false;
    double startedAt=0,stoppedAt=0,lastPollAt=0;
};
}
