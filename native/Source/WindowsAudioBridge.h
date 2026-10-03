#pragma once

#include <juce_core/juce_core.h>

namespace fengyin
{
struct AudioBridgeStatus
{
    bool supported = false;
    bool installed = false;
    bool active = false;
    bool running = false;
    bool restartRequired = false;
    bool canRestore = false;
    juce::String state = "idle";
    juce::String message;
};

// Test-only bridge: FengYin -> VoiceMeeter Virtual ASIO -> ASIO4ALL -> OEM device,
// while Windows applications enter VoiceMeeter through its shared WDM endpoint.
class WindowsAudioBridge final
{
public:
    AudioBridgeStatus getStatus() const;
    bool launchInstaller();
    juce::String configureVoiceMeeter();
    juce::String routeWindowsAudioToVoiceMeeter();
    juce::String restoreWindowsAudio();
    juce::String findVoiceMeeterAsioName() const;

private:
    juce::File stateFile() const;
    juce::File installerScript() const;
    juce::File voiceMeeterRemoteDll() const;
};
}
