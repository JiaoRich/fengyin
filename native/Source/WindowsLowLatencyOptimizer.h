#pragma once

#include <juce_core/juce_core.h>

namespace fengyin
{
struct WindowsLowLatencyStatus
{
    bool supported = false;
    bool eligibleDeviceFound = false;
    bool running = false;
    bool canRestore = false;
    bool restartRequired = false;
    juce::String deviceName;
    juce::String state { "idle" };
    juce::String message;
};

// Reversible wrapper around the Windows inbox High Definition Audio driver.
// The privileged work is performed by an embedded PowerShell script so every
// system mutation is visible in one auditable place and can be rolled back.
class WindowsLowLatencyOptimizer final
{
public:
    WindowsLowLatencyStatus getStatus() const;
    bool launchOptimisation();
    bool launchRestore();

private:
    static juce::File getWorkDirectory();
    static juce::File getScriptFile();
    static juce::File getResultFile();
    static juce::File getManifestFile();
    static bool launchElevated(const juce::String& mode);
    static bool writeEmbeddedScript();
};
}
