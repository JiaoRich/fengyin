#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

namespace fengyin {
// Capture/encoding runs in a separate Windows helper, never the audio callback.
class WindowRecordingService {
public:
    ~WindowRecordingService();
    bool start(void* window, juce::Colour background, bool rounded, bool audioOnly=false);
    bool isAudioOnly() const noexcept {return audioMode;}
    double elapsedSeconds() const noexcept {return running?(juce::Time::getMillisecondCounterHiRes()-startedAt)/1000.0:0.0;}
    void stop();
    void poll();
    bool busy() const noexcept {return running;}
    bool stopping() const noexcept {return finishing;}
    bool recording() const noexcept {return running && !finishing && ready;}
    juce::String error() const {return lastError;}
    juce::File lastFile() const {return destination;}
    bool takeCompleted() noexcept {const bool v=completed;completed=false;return v;}
private:
    juce::ChildProcess process;
    juce::File destination, stopFile, stateFile, partialFile;
    juce::String lastError;
    bool running=false,finishing=false,ready=false,completed=false,audioMode=false;
    double startedAt=0,stoppedAt=0;
};
}
