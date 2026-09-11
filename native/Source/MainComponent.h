#pragma once

#include <juce_gui_extra/juce_gui_extra.h>
#include "AudioDeviceService.h"
#include "MidiInputService.h"
#include "TestSynthEngine.h"
#include "PluginCatalogService.h"
#include "PluginHostEngine.h"
#include "SoundPresetStore.h"
#include "VideoPlayerPanel.h"
#include "LicenseService.h"
#include "RecordingService.h"
#include "AccompanimentAudioService.h"
#include "MasterOutputService.h"

class MainComponent final : public juce::Component, private juce::Timer
{
public:
    MainComponent();
    ~MainComponent() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    enum class Page { play, sounds, chain, wind, audio, settings };
    enum class Theme { neon = 1, gold, minimal };
    void showPage(Page page);
    void updatePageVisibility();
    void applyTheme(Theme theme);
    void setupWebInterface();
    static std::optional<juce::WebBrowserComponent::Resource> getWebResource(const juce::String& path);
    juce::Rectangle<int> getContentBounds() const;
    void timerCallback() override;
    void startPluginScan();
    void refreshPluginChoices();
    void loadSelectedPlugin();
    void loadSelectedEffect();
    void removeEffect();
    void useTestSynth();
    void refreshPresetChoices();
    void saveCurrentPreset();
    void commitCurrentPreset(const juce::String& name);
    void loadSelectedPreset();
    void togglePresetFavorite();
    void makePresetDefault();
    void deleteSelectedPreset();
    void tryAutoLoadDefaultPreset();
    void showRecordingManager();
    void showSetupGuide(bool automatic = false);
    static juce::File getOnboardingMarkerFile();
    void activatePluginOutput(const juce::String& pluginName);
    void showActivationDialog();
    void refreshLicenseUi();
    void toggleRecording();
    void showDeviceSettings();
    void showMidiSetup();
    void startBreathDetection(const juce::String& deviceIdentifier);
    void showExpressionSettings();
    static juce::String midiNoteName(int note);

    fengyin::MidiInputService midi;
    fengyin::AudioDeviceService audio;
    fengyin::AccompanimentAudioService accompaniment;
    fengyin::RecordingService recorder;
    fengyin::MasterOutputService masterOutput;
    fengyin::TestSynthEngine testSynth;
    fengyin::PluginCatalogService pluginCatalog;
    fengyin::PluginHostEngine pluginHost;
    fengyin::SoundPresetStore presetStore;
    fengyin::LicenseService license;
    fengyin::MidiSnapshot snapshot;
    juce::TextButton detectButton;
    juce::TextButton settingsButton;
    juce::TextButton expressionButton;
    juce::TextButton helpButton;
    juce::Label title;
    juce::TextButton playNav;
    juce::TextButton soundsNav;
    juce::TextButton chainNav;
    juce::TextButton windNav;
    juce::TextButton audioNav;
    juce::TextButton softwareNav;
    juce::ComboBox themeSelector;
    juce::ToggleButton lowPerformanceToggle;
    juce::Label deviceStatus;
    juce::Label audioStatus;
    juce::TextButton licenseButton;
    juce::Label noteLabel;
    juce::Label breathLabel;
    juce::TextButton scanPluginsButton;
    juce::ComboBox pluginSelector;
    juce::TextButton loadPluginButton;
    juce::TextButton pluginEditorButton;
    juce::Label pluginStatus;
    juce::ComboBox effectSelector;
    juce::TextButton loadEffectButton;
    juce::TextButton removeEffectButton;
    juce::TextButton effectEditorButton;
    juce::TextButton bypassEffectButton;
    juce::Label effectStatus;
    juce::ComboBox presetSelector;
    juce::TextButton savePresetButton;
    juce::TextButton loadPresetButton;
    juce::TextButton favoritePresetButton;
    juce::TextButton defaultPresetButton;
    juce::TextButton deletePresetButton;
    juce::TextButton recordButton;
    juce::Label recordingStatus;
    juce::Slider masterVolume;
    juce::Label masterVolumeLabel;
    juce::TextButton recordingManagerButton;
    juce::Array<fengyin::SoundPreset> cachedPresets;
    juce::Array<juce::PluginDescription> cachedInstrumentPlugins;
    juce::Array<juce::PluginDescription> cachedEffectPlugins;
    fengyin::VideoPlayerPanel videoPlayer;
    bool pluginChoicesLoaded = false;
    bool attemptedDefaultPreset = false;
    bool isActivated = false;
    std::unique_ptr<juce::AlertWindow> activationDialog;
    std::unique_ptr<juce::AlertWindow> deviceDialog;
    std::unique_ptr<juce::AlertWindow> midiDialog;
    std::unique_ptr<juce::AlertWindow> expressionDialog;
    std::unique_ptr<juce::AlertWindow> savePresetDialog;
    std::unique_ptr<juce::AlertWindow> recordingManagerDialog;
    std::unique_ptr<juce::AlertWindow> setupGuideDialog;
    juce::Array<juce::File> managedRecordings;
    std::vector<juce::MidiDeviceInfo> midiDialogDevices;
    bool breathDetectionActive = false;
    double breathDetectionEndsAtMs = 0.0;
    double autoGuideAtMs = 0.0;
    bool autoGuideShown = false;
    float simulatedPhase = 0.0f;
    float displayedLeftPeak = 0.0f;
    float displayedRightPeak = 0.0f;
    std::array<float, fengyin::MasterOutputService::spectrumBands> spectrumLevels {};
    Page currentPage = Page::play;
    Theme currentTheme = Theme::neon;
    juce::Colour themeBackground { 0xff07101d };
    juce::Colour themePanel { 0xff102238 };
    juce::Colour themePanelBright { 0xff142b46 };
    juce::Colour themeAccent { 0xff43d9ff };
    juce::Colour themeAccent2 { 0xff9b63ff };
    juce::Colour themeMuted { 0xff8fa7bd };
    std::unique_ptr<juce::WebBrowserComponent> webInterface;
    bool webInterfaceReady = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};
