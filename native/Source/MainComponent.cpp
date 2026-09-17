#include "MainComponent.h"
#include "BinaryData.h"
#include <algorithm>
#include <cstring>

namespace
{
const juce::Colour background { 0xff07101d };
const juce::Colour panel { 0xff102238 };
const juce::Colour cyan { 0xff43d9ff };
const juce::Colour violet { 0xff9b63ff };
juce::String utf8(const char* text) { return juce::String::fromUTF8(text); }
const auto licensePublicKey = juce::String("5,d47f8d2272ed935eb504695cc78aa24a67e8b7a006c2e62e31c047727e7963e836cc0e36bf528b328e4eb0e73cacdc7a52160c961027a7fc7c77195d8a8e3568ca07e82303a9cb256b5627ea1ad8815a224801576ac89b548030474b1743f7e067cbd2ade4cc983a1973ca0a775b4c7c84e863ddf5cabb49ab5d684b8672fdad");
}

MainComponent::MainComponent() : license(licensePublicKey), machineCode(license.getMachineCode())
{
    setOpaque(true);
    setSize(1440, 900);

    const auto setupNav = [this](juce::TextButton& button, const char* text, Page page)
    {
        button.setButtonText(utf8(text));
        button.setClickingTogglesState(false);
        button.onClick = [this, page] { showPage(page); };
        addAndMakeVisible(button);
    };
    setupNav(playNav, "◉  开始演奏", Page::play);
    setupNav(soundsNav, "♫  音色方案", Page::sounds);
    setupNav(chainNav, "◇  音源与音效", Page::chain);
    setupNav(windNav, "✦  智能适配", Page::wind);
    setupNav(audioNav, "▣  声音设置", Page::audio);
    setupNav(softwareNav, "⚙  软件设置", Page::settings);

    themeSelector.addItem(utf8("霓虹舞台"), static_cast<int>(Theme::neon));
    themeSelector.addItem(utf8("金色大厅"), static_cast<int>(Theme::gold));
    themeSelector.addItem(utf8("极简专业"), static_cast<int>(Theme::minimal));
    themeSelector.setSelectedId(static_cast<int>(Theme::neon), juce::dontSendNotification);
    themeSelector.onChange = [this] { applyTheme(static_cast<Theme>(themeSelector.getSelectedId())); };
    addAndMakeVisible(themeSelector);

    lowPerformanceToggle.setButtonText(utf8("低性能电脑模式"));
    lowPerformanceToggle.onClick = [this] { repaint(); };
    addAndMakeVisible(lowPerformanceToggle);

    title.setText(utf8("风吟 · 开始演奏"), juce::dontSendNotification);
    title.setFont(juce::FontOptions(28.0f, juce::Font::bold));
    title.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(title);

    deviceStatus.setColour(juce::Label::textColourId, juce::Colour(0xff8fa7bd));
    addAndMakeVisible(deviceStatus);

    audioStatus.setJustificationType(juce::Justification::centredRight);
    audioStatus.setColour(juce::Label::textColourId, juce::Colour(0xff8fa7bd));
    addAndMakeVisible(audioStatus);

    licenseButton.onClick = [this] { showActivationDialog(); };
    addAndMakeVisible(licenseButton);

    noteLabel.setJustificationType(juce::Justification::centred);
    noteLabel.setFont(juce::FontOptions(76.0f, juce::Font::bold));
    noteLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(noteLabel);

    breathLabel.setJustificationType(juce::Justification::centred);
    breathLabel.setFont(juce::FontOptions(18.0f));
    breathLabel.setColour(juce::Label::textColourId, cyan);
    addAndMakeVisible(breathLabel);

    detectButton.setButtonText(utf8("连接向导"));
    settingsButton.setButtonText(utf8("声音设置"));
    settingsButton.onClick = [this] { showDeviceSettings(); };
    expressionButton.setButtonText(utf8("吹奏调节"));
    expressionButton.onClick = [this] { showExpressionSettings(); };
    helpButton.setButtonText(utf8("使用向导"));
    helpButton.onClick = [this] { showSetupGuide(false); };

    scanPluginsButton.setButtonText(utf8("扫描音源"));
    scanPluginsButton.onClick = [this] { startPluginScan(); };
    addAndMakeVisible(scanPluginsButton);

    pluginSelector.setTextWhenNothingSelected(utf8("请选择 VST3 / SWAM 音源"));
    pluginSelector.setTextWhenNoChoicesAvailable(utf8("尚未扫描到音源"));
    addAndMakeVisible(pluginSelector);

    loadPluginButton.setButtonText(utf8("加载音源"));
    loadPluginButton.onClick = [this] { loadSelectedPlugin(); };
    addAndMakeVisible(loadPluginButton);
    pluginEditorButton.setButtonText(utf8("打开音源"));
    pluginEditorButton.onClick = [this] { pluginHost.showPluginEditor(false); };
    addAndMakeVisible(pluginEditorButton);

    pluginStatus.setText(utf8("当前使用：安全测试音源"), juce::dontSendNotification);
    pluginStatus.setColour(juce::Label::textColourId, juce::Colour(0xff8fa7bd));
    addAndMakeVisible(pluginStatus);

    effectSelector.setTextWhenNothingSelected(utf8("请选择效果器（可不选）"));
    effectSelector.setTextWhenNoChoicesAvailable(utf8("没有扫描到效果器"));
    addAndMakeVisible(effectSelector);
    loadEffectButton.setButtonText(utf8("加载效果"));
    loadEffectButton.onClick = [this] { loadSelectedEffect(); };
    addAndMakeVisible(loadEffectButton);
    removeEffectButton.setButtonText(utf8("移除效果"));
    removeEffectButton.onClick = [this] { removeEffect(); };
    addAndMakeVisible(removeEffectButton);
    effectEditorButton.setButtonText(utf8("打开界面"));
    effectEditorButton.onClick = [this] { pluginHost.showPluginEditor(true); };
    addAndMakeVisible(effectEditorButton);
    bypassEffectButton.setButtonText(utf8("旁通"));
    bypassEffectButton.setClickingTogglesState(true);
    bypassEffectButton.onClick = [this]
    {
        pluginHost.setEffectBypassed(bypassEffectButton.getToggleState());
        bypassEffectButton.setButtonText(bypassEffectButton.getToggleState() ? utf8("已旁通") : utf8("旁通"));
    };
    addAndMakeVisible(bypassEffectButton);
    effectStatus.setText(utf8("效果器：未使用"), juce::dontSendNotification);
    effectStatus.setColour(juce::Label::textColourId, juce::Colour(0xff8fa7bd));
    addAndMakeVisible(effectStatus);
    addAndMakeVisible(videoPlayer);

    presetSelector.setTextWhenNothingSelected(utf8("请选择已保存的音色方案"));
    presetSelector.setTextWhenNoChoicesAvailable(utf8("尚未保存音色方案"));
    addAndMakeVisible(presetSelector);

    savePresetButton.setButtonText(utf8("保存当前音色"));
    savePresetButton.onClick = [this] { saveCurrentPreset(); };
    addAndMakeVisible(savePresetButton);

    loadPresetButton.setButtonText(utf8("载入方案"));
    loadPresetButton.onClick = [this] { loadSelectedPreset(); };
    addAndMakeVisible(loadPresetButton);
    favoritePresetButton.setButtonText(utf8("收藏"));
    favoritePresetButton.onClick = [this] { togglePresetFavorite(); };
    addAndMakeVisible(favoritePresetButton);
    defaultPresetButton.setButtonText(utf8("设为默认"));
    defaultPresetButton.onClick = [this] { makePresetDefault(); };
    addAndMakeVisible(defaultPresetButton);
    deletePresetButton.setButtonText(utf8("删除"));
    deletePresetButton.onClick = [this] { deleteSelectedPreset(); };
    addAndMakeVisible(deletePresetButton);

    recordButton.setButtonText(utf8("● 开始录音"));
    recordButton.onClick = [this] { toggleRecording(); };
    addAndMakeVisible(recordButton);
    recordingStatus.setColour(juce::Label::textColourId, juce::Colour(0xff8fa7bd));
    addAndMakeVisible(recordingStatus);
    masterVolume.setSliderStyle(juce::Slider::LinearHorizontal);
    masterVolume.setTextBoxStyle(juce::Slider::TextBoxRight, false, 64, 26);
    masterVolume.setRange(0.0, 1.2, 0.01);
    masterVolume.setValue(0.8, juce::dontSendNotification);
    masterVolume.setTextValueSuffix(utf8(" 倍"));
    masterVolume.onValueChange = [this] { masterOutput.setGain(static_cast<float>(masterVolume.getValue())); };
    addAndMakeVisible(masterVolume);
    masterVolumeLabel.setText(utf8("总音量"), juce::dontSendNotification);
    masterVolumeLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(masterVolumeLabel);
    recordingManagerButton.setButtonText(utf8("录音文件"));
    recordingManagerButton.onClick = [this] { showRecordingManager(); };
    addAndMakeVisible(recordingManagerButton);

    detectButton.onClick = [this] { showMidiSetup(); };
    addAndMakeVisible(detectButton);
    addAndMakeVisible(settingsButton);
    addAndMakeVisible(expressionButton);
    addAndMakeVisible(helpButton);

    midi.refreshAndConnectFirstAvailable();
    audio.initialise();
    audio.applyBestInitialSetup();
    testSynth.setRecordingService(&recorder);
    testSynth.setAccompanimentService(&accompaniment);
    pluginHost.setRecordingService(&recorder);
    pluginHost.setAccompanimentService(&accompaniment);
    testSynth.setMasterOutputService(&masterOutput);
    pluginHost.setMasterOutputService(&masterOutput);
    videoPlayer.setAccompanimentService(&accompaniment);
    midi.setPerformanceSink(&testSynth);
    audio.getDeviceManager().addAudioCallback(&testSynth);
    refreshPresetChoices();
    refreshLicenseUi();
    if (! pluginCatalog.getPlugins().isEmpty()) refreshPluginChoices();
    applyTheme(Theme::neon);
    showPage(Page::play);
    setupWebInterface();
    startTimerHz(30);
    // 网页主界面本身包含完整引导，不再启动会遮挡演奏页面的原生模态窗口。
    autoGuideShown = true;
    autoGuideAtMs = 0.0;
}

MainComponent::~MainComponent()
{
    stopTimer();
    webVideoAudioExtractor.cancel();
    accompaniment.pause();
    webInterface.reset();
    recorder.stop();
    midi.setPerformanceSink(nullptr);
    pluginHost.detach();
    audio.getDeviceManager().removeAudioCallback(&testSynth);
}

void MainComponent::setupWebInterface()
{
    auto options = juce::WebBrowserComponent::Options{}
        .withBackend(juce::WebBrowserComponent::Options::Backend::webview2)
        .withWinWebView2Options(juce::WebBrowserComponent::Options::WinWebView2{}
            .withUserDataFolder(juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                .getChildFile("FengYin").getChildFile("WebView2"))
            .withStatusBarDisabled()
            .withBackgroundColour(juce::Colour(0xff07101d)))
        .withNativeIntegrationEnabled()
        .withEventListener("webReady", [this](juce::var)
        {
            webInterfaceReady = true;
            emitAudioSettingsState();
        })
        .withEventListener("scanPlugins", [this](juce::var) { if (isActivated) startPluginScan(); })
        .withEventListener("loadPlugin", [this](juce::var payload)
        {
            const auto index = static_cast<int>(payload.getProperty("index", -1));
            if (isActivated && juce::isPositiveAndBelow(index, cachedInstrumentPlugins.size()))
            {
                pluginSelector.setSelectedId(index + 1, juce::dontSendNotification);
                loadSelectedPlugin();
            }
        })
        .withEventListener("openPlugin", [this](juce::var)
        {
            const auto opened = isActivated && pluginHost.showPluginEditor(false);
            if (webInterface != nullptr)
                webInterface->emitEventIfBrowserIsVisible("editorResult", opened ? utf8("音源界面已打开") : utf8("请先加载一个音源"));
        })
        .withEventListener("loadEffect", [this](juce::var payload)
        {
            const auto index = static_cast<int>(payload.getProperty("index", -1));
            if (isActivated && juce::isPositiveAndBelow(index, cachedEffectPlugins.size()))
            {
                effectSelector.setSelectedId(index + 1, juce::dontSendNotification);
                loadSelectedEffect();
            }
        })
        .withEventListener("removeEffect", [this](juce::var) { if (isActivated) removeEffect(); })
        .withEventListener("openEffect", [this](juce::var)
        {
            const auto opened = isActivated && pluginHost.showPluginEditor(true);
            if (webInterface != nullptr)
                webInterface->emitEventIfBrowserIsVisible("editorResult", opened ? utf8("效果器界面已打开") : utf8("请先加载一个外部效果器"));
        })
        .withEventListener("savePreset", [this](juce::var) { if (isActivated && pluginHost.hasPlugin()) saveCurrentPreset(); })
        .withEventListener("loadPreset", [this](juce::var payload)
        {
            const auto index = static_cast<int>(payload.getProperty("index", -1));
            if (isActivated && juce::isPositiveAndBelow(index, cachedPresets.size()))
            {
                editingPresetId.clear();
                presetSelector.setSelectedId(index + 1, juce::dontSendNotification);
                loadSelectedPreset([this](bool success, const juce::String& message)
                {
                    if (webInterface == nullptr) return;
                    auto result = std::make_unique<juce::DynamicObject>();
                    result->setProperty("success", success);
                    result->setProperty("message", message);
                    webInterface->emitEventIfBrowserIsVisible("presetLoadResult", juce::var(result.release()));
                });
            }
        })
        .withEventListener("editPreset", [this](juce::var payload)
        {
            const auto index = static_cast<int>(payload.getProperty("index", -1));
            if (isActivated && juce::isPositiveAndBelow(index, cachedPresets.size()))
            {
                editingPresetId = cachedPresets.getReference(index).id;
                presetSelector.setSelectedId(index + 1, juce::dontSendNotification);
                loadSelectedPreset([this](bool success, const juce::String& message)
                {
                    if (! success) editingPresetId.clear();
                    if (webInterface == nullptr) return;
                    auto result = std::make_unique<juce::DynamicObject>();
                    result->setProperty("success", success);
                    result->setProperty("message", message);
                    webInterface->emitEventIfBrowserIsVisible("presetLoadResult", juce::var(result.release()));
                });
            }
        })
        .withEventListener("cancelPresetEdit", [this](juce::var) { editingPresetId.clear(); })
        .withEventListener("deletePreset", [this](juce::var payload)
        {
            const auto index = static_cast<int>(payload.getProperty("index", -1));
            if (isActivated && juce::isPositiveAndBelow(index, cachedPresets.size()))
            {
                presetSelector.setSelectedId(index + 1, juce::dontSendNotification);
                deleteSelectedPreset();
            }
        })
        .withEventListener("setMasterVolume", [this](juce::var payload)
        {
            masterOutput.setGain(static_cast<float>(static_cast<double>(payload.getProperty("value", 0.8))));
        })
        .withEventListener("setTranspose", [this](juce::var payload)
        {
            midi.setTransposeSemitones(static_cast<int>(payload.getProperty("semitones", 0)));
        })
        .withEventListener("setKeyTranspose", [this](juce::var payload)
        {
            midi.setTargetKey(static_cast<int>(payload.getProperty("targetKey", 0)));
        })
        .withEventListener("beginKeyCalibration", [this](juce::var)
        {
            midi.beginKeyCalibration();
        })
        .withEventListener("cancelKeyCalibration", [this](juce::var)
        {
            midi.cancelKeyCalibration();
        })
        .withEventListener("copyMachineCode", [this](juce::var)
        {
            juce::SystemClipboard::copyTextToClipboard(machineCode);
        })
        .withEventListener("chooseVideo", [this](juce::var) { chooseVideoForWebInterface(); })
        .withEventListener("setVideoPlaybackState", [this](juce::var payload)
        {
            videoPlaybackActive = static_cast<bool>(payload.getProperty("playing", false));
            webVideoPosition = static_cast<double>(payload.getProperty("position", webVideoPosition));
            audioOutputSyncTicks = 0;
            if (videoPlaybackActive)
            {
                audio.followSystemDefaultOutput();
                if (webVideoAudioReady)
                {
                    accompaniment.setPosition(webVideoPosition);
                    accompaniment.play();
                }
            }
            else
                accompaniment.pause();
        })
        .withEventListener("syncVideoPlayback", [this](juce::var payload)
        {
            webVideoPosition = static_cast<double>(payload.getProperty("position", webVideoPosition));
            if (webVideoAudioReady && std::abs(accompaniment.getPosition() - webVideoPosition) > 0.35)
                accompaniment.setPosition(webVideoPosition);
        })
        .withEventListener("seekVideo", [this](juce::var payload)
        {
            webVideoPosition = static_cast<double>(payload.getProperty("position", 0.0));
            if (webVideoAudioReady) accompaniment.setPosition(webVideoPosition);
        })
        .withEventListener("setVideoVolume", [this](juce::var payload)
        {
            webVideoVolume = juce::jlimit(0.0f, 1.0f,
                static_cast<float>(static_cast<double>(payload.getProperty("value", 1.0))));
            accompaniment.setVolume(webVideoVolume);
        })
        .withEventListener("setPerformanceReverb", [this](juce::var payload)
        {
            masterOutput.setReverbMix(static_cast<float>(static_cast<double>(payload.getProperty("value", 0.28))));
        })
        .withEventListener("setSmartOptimisation", [this](juce::var payload)
        {
            const auto enabled = static_cast<bool>(payload.getProperty("enabled", true));
            masterOutput.setSmartOptimisationEnabled(enabled);
            if (enabled && webInterface != nullptr)
                webInterface->emitEventIfBrowserIsVisible("audioOptimisationResult", audio.optimiseForLivePerformance());
        })
        .withEventListener("requestAudioSettings", [this](juce::var) { emitAudioSettingsState(); })
        .withEventListener("applyAudioSettings", [this](juce::var payload) { applyAudioSettingsFromWeb(payload); })
        .withEventListener("optimiseAudioSettings", [this](juce::var)
        {
            if (recorder.isRecording()) toggleRecording();
            const auto message = audio.optimiseForLivePerformance();
            emitAudioSettingsState(message.containsIgnoreCase(utf8("已启用")), message);
        })
        .withEventListener("beginTechniqueLearn", [this](juce::var payload)
        {
            const auto technique = juce::jlimit(0, static_cast<int>(fengyin::PerformanceTechnique::count) - 1,
                                                static_cast<int>(payload.getProperty("technique", 0)));
            if (! midi.getSnapshot().deviceConnected)
            {
                if (webInterface != nullptr)
                {
                    auto result = std::make_unique<juce::DynamicObject>();
                    result->setProperty("success", false);
                    result->setProperty("message", utf8("请先连接电吹管"));
                    webInterface->emitEventIfBrowserIsVisible("techniqueLearnResult", juce::var(result.release()));
                }
                return;
            }
            const auto family = pluginHost.hasPlugin()
                ? fengyin::SwamPluginClassifier::classify(pluginHost.getPluginName().toStdString(), {})
                : fengyin::SwamFamily::notSwam;
            const auto target = static_cast<fengyin::PerformanceTechnique>(technique);
            const auto advice = fengyin::TechniqueAdvisor::advise(midi.getActiveProfile(), family, target);
            if (! pluginHost.hasPlugin() || ! advice.relevantToInstrument || ! pluginHost.supportsTechnique(target))
            {
                if (webInterface != nullptr)
                {
                    auto result = std::make_unique<juce::DynamicObject>();
                    result->setProperty("success", false);
                    result->setProperty("message", ! advice.relevantToInstrument ? utf8("当前乐器不需要这项技巧")
                                                                                : utf8("当前音源没有开放这项技巧参数"));
                    webInterface->emitEventIfBrowserIsVisible("techniqueLearnResult", juce::var(result.release()));
                }
                return;
            }
            activeTechniqueLearn = technique;
            pendingTechniqueToggle = static_cast<bool>(payload.getProperty("toggle", false));
            techniqueLearnEndsAtMs = juce::Time::getMillisecondCounterHiRes() + 10000.0;
            midi.beginTechniqueLearn(static_cast<fengyin::PerformanceTechnique>(technique));
        })
        .withEventListener("cancelTechniqueLearn", [this](juce::var)
        {
            midi.cancelTechniqueLearn();
            activeTechniqueLearn = -1;
            techniqueLearnEndsAtMs = 0.0;
        })
        .withEventListener("removeTechniqueMapping", [this](juce::var payload)
        {
            const auto technique = static_cast<int>(payload.getProperty("technique", 0));
            auto mappings = midi.getTechniqueMappings();
            for (int index = mappings.size(); --index >= 0;)
                if (static_cast<int>(mappings.getReference(index).technique) == technique)
                    mappings.remove(index);
            midi.setTechniqueMappings(mappings);
        })
        .withEventListener("setBuiltinEffects", [this](juce::var payload)
        {
            masterOutput.setEqTone(static_cast<float>(static_cast<double>(payload.getProperty("eq", 0.2))));
            masterOutput.setReverbMix(static_cast<float>(static_cast<double>(payload.getProperty("reverb", 0.28))));
            masterOutput.setLimiterCeiling(static_cast<float>(static_cast<double>(payload.getProperty("limiter", 0.95))));
        })
        .withEventListener("showMidiSetup", [this](juce::var) { showMidiSetup(); })
        .withEventListener("showExpressionSettings", [this](juce::var) { showExpressionSettings(); })
        .withEventListener("showAudioSettings", [this](juce::var) { showDeviceSettings(); })
        .withEventListener("toggleRecording", [this](juce::var) { toggleRecording(); })
        .withEventListener("showRecordings", [](juce::var)
        {
            const auto folder = fengyin::RecordingService::getRecordingsFolder();
            folder.createDirectory();
            folder.revealToUser();
        })
        .withEventListener("activate", [this](juce::var payload)
        {
            const auto status = license.activate(payload.getProperty("code", juce::String()).toString());
            refreshLicenseUi();
            if (webInterface != nullptr)
            {
                auto result = std::make_unique<juce::DynamicObject>();
                result->setProperty("activated", status.activated);
                result->setProperty("message", status.message);
                webInterface->emitEventIfBrowserIsVisible("activationResult", juce::var(result.release()));
            }
        })
        .withEventListener("showActivation", [this](juce::var) { showActivationDialog(); })
        .withResourceProvider([](const juce::String& path) { return getWebResource(path); });

    webInterface = std::make_unique<juce::WebBrowserComponent>(options);
    addAndMakeVisible(*webInterface);
    // The component may have received its first resized() callback before the browser existed.
    // Give WebView2 a real viewport immediately so the legacy native UI never appears at startup.
    webInterface->setBounds(getLocalBounds());
    const auto localInterface = prepareLocalWebInterface();
    webInterface->goToURL(localInterface.existsAsFile()
        ? juce::URL(localInterface).toString(false)
        : juce::WebBrowserComponent::getResourceProviderRoot());
    webInterface->toFront(false);
}

juce::File MainComponent::prepareLocalWebInterface()
{
    const auto root = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                          .getChildFile("FengYin").getChildFile("web-runtime");
    const juce::StringArray files {
        "prototype/index.html", "prototype/styles.css", "prototype/app.js", "assets/fengyin-app-icon.png",
        "assets/instruments/instrument_soprano_sax.png", "assets/instruments/instrument_alto_sax.png",
        "assets/instruments/instrument_tenor_sax.png", "assets/instruments/instrument_baritone_sax.png",
        "assets/instruments/instrument_flugelhorn.png", "assets/instruments/instrument_trumpet.png",
        "assets/instruments/instrument_piccolo_trumpet.png", "assets/instruments/instrument_tenor_trombone.png",
        "assets/instruments/instrument_bass_trombone.png", "assets/instruments/instrument_tuba.png",
        "assets/instruments/instrument_euphonium.png", "assets/instruments/instrument_horn.png",
        "assets/instruments/instrument_piccolo.png", "assets/instruments/instrument_flute.png",
        "assets/instruments/instrument_alto_flute.png", "assets/instruments/instrument_bass_flute.png",
        "assets/instruments/instrument_clarinet.png", "assets/instruments/instrument_bass_clarinet.png",
        "assets/instruments/instrument_oboe.png", "assets/instruments/instrument_english_horn.png",
        "assets/instruments/instrument_bassoon.png", "assets/instruments/instrument_contrabassoon.png",
        "assets/instruments/instrument_violin.png", "assets/instruments/instrument_viola.png",
        "assets/instruments/instrument_cello.png", "assets/instruments/instrument_double_bass.png"
    };
    for (const auto& relative : files)
    {
        const auto resourcePath = relative.startsWith("prototype/") ? relative.fromFirstOccurrenceOf("prototype/", false, false) : relative;
        const auto resource = getWebResource("/" + resourcePath);
        if (! resource.has_value()) return {};
        const auto destination = root.getChildFile(relative);
        destination.getParentDirectory().createDirectory();
        if (! destination.replaceWithData(resource->data.data(), resource->data.size())) return {};
    }
    return root.getChildFile("prototype").getChildFile("index.html");
}

void MainComponent::chooseVideoForWebInterface()
{
    webVideoChooser = std::make_unique<juce::FileChooser>(utf8("选择伴奏视频"), juce::File(), "*.mp4;*.mov;*.m4v;*.avi;*.mkv;*.webm");
    const auto safeThis = juce::Component::SafePointer<MainComponent>(this);
    webVideoChooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [safeThis](const juce::FileChooser& chooser)
        {
            if (safeThis == nullptr) return;
            const auto file = chooser.getResult();
            safeThis->webVideoChooser.reset();
            if (file.existsAsFile()) safeThis->loadVideoForWebInterface(file);
        });
}

void MainComponent::loadVideoForWebInterface(const juce::File& file)
{
    webVideoAudioExtractor.cancel();
    accompaniment.unload();
    webVideoFile = file;
    webVideoPosition = 0.0;
    webVideoAudioReady = false;

    if (webInterface != nullptr)
    {
        auto result = std::make_unique<juce::DynamicObject>();
        result->setProperty("url", juce::URL(file).toString(false));
        result->setProperty("name", file.getFileName());
        webInterface->emitEventIfBrowserIsVisible("videoSelected", juce::var(result.release()));
    }

    if (accompaniment.loadForVideo(file))
    {
        accompaniment.setVolume(webVideoVolume);
        webVideoAudioReady = true;
        emitVideoAudioState(true, utf8("伴奏声音已进入风吟混音与录音"));
        return;
    }

    emitVideoAudioState(false, utf8("正在准备伴奏音轨，请稍候…"));
    webVideoAudioExtractor.extractAsync(file,
        [safeThis = juce::Component::SafePointer<MainComponent>(this), file](bool success, const juce::File& audioFile, const juce::String& error)
        {
            if (safeThis == nullptr || safeThis->webVideoFile != file) return;
            const auto loaded = success && safeThis->accompaniment.loadAudioFile(audioFile);
            safeThis->webVideoAudioReady = loaded;
            if (loaded)
            {
                safeThis->accompaniment.setVolume(safeThis->webVideoVolume);
                safeThis->accompaniment.setPosition(safeThis->webVideoPosition);
                if (safeThis->videoPlaybackActive) safeThis->accompaniment.play();
                safeThis->emitVideoAudioState(true, utf8("伴奏声音已进入风吟混音与录音"));
            }
            else
                safeThis->emitVideoAudioState(false, error.isNotEmpty() ? error : utf8("无法读取视频中的伴奏音轨"));
        });
}

void MainComponent::emitVideoAudioState(bool ready, const juce::String& message)
{
    if (webInterface == nullptr) return;
    auto result = std::make_unique<juce::DynamicObject>();
    result->setProperty("ready", ready);
    result->setProperty("message", message);
    webInterface->emitEventIfBrowserIsVisible("videoAudioState", juce::var(result.release()));
}

std::optional<juce::WebBrowserComponent::Resource> MainComponent::getWebResource(const juce::String& path)
{
    const auto requested = path == "/" ? juce::String("index.html") : path.trimCharactersAtStart("/");
    const void* data = nullptr;
    int size = 0;
    juce::String mime;
    if (requested == "index.html") { data = BinaryData::index_html; size = BinaryData::index_htmlSize; mime = "text/html"; }
    else if (requested == "styles.css") { data = BinaryData::styles_css; size = BinaryData::styles_cssSize; mime = "text/css"; }
    else if (requested == "app.js") { data = BinaryData::app_js; size = BinaryData::app_jsSize; mime = "text/javascript"; }
    else if (requested == "fengyin-app-icon.png" || requested == "assets/fengyin-app-icon.png")
    {
        data = BinaryData::fengyinappicon_png; size = BinaryData::fengyinappicon_pngSize; mime = "image/png";
    }
    else
    {
        struct ImageResource { const char* file; const char* data; int size; };
        const ImageResource images[] {
            { "instrument_soprano_sax.png", BinaryData::instrument_soprano_sax_png, BinaryData::instrument_soprano_sax_pngSize },
            { "instrument_alto_sax.png", BinaryData::instrument_alto_sax_png, BinaryData::instrument_alto_sax_pngSize },
            { "instrument_tenor_sax.png", BinaryData::instrument_tenor_sax_png, BinaryData::instrument_tenor_sax_pngSize },
            { "instrument_baritone_sax.png", BinaryData::instrument_baritone_sax_png, BinaryData::instrument_baritone_sax_pngSize },
            { "instrument_flugelhorn.png", BinaryData::instrument_flugelhorn_png, BinaryData::instrument_flugelhorn_pngSize },
            { "instrument_trumpet.png", BinaryData::instrument_trumpet_png, BinaryData::instrument_trumpet_pngSize },
            { "instrument_piccolo_trumpet.png", BinaryData::instrument_piccolo_trumpet_png, BinaryData::instrument_piccolo_trumpet_pngSize },
            { "instrument_tenor_trombone.png", BinaryData::instrument_tenor_trombone_png, BinaryData::instrument_tenor_trombone_pngSize },
            { "instrument_bass_trombone.png", BinaryData::instrument_bass_trombone_png, BinaryData::instrument_bass_trombone_pngSize },
            { "instrument_tuba.png", BinaryData::instrument_tuba_png, BinaryData::instrument_tuba_pngSize },
            { "instrument_euphonium.png", BinaryData::instrument_euphonium_png, BinaryData::instrument_euphonium_pngSize },
            { "instrument_horn.png", BinaryData::instrument_horn_png, BinaryData::instrument_horn_pngSize },
            { "instrument_piccolo.png", BinaryData::instrument_piccolo_png, BinaryData::instrument_piccolo_pngSize },
            { "instrument_flute.png", BinaryData::instrument_flute_png, BinaryData::instrument_flute_pngSize },
            { "instrument_alto_flute.png", BinaryData::instrument_alto_flute_png, BinaryData::instrument_alto_flute_pngSize },
            { "instrument_bass_flute.png", BinaryData::instrument_bass_flute_png, BinaryData::instrument_bass_flute_pngSize },
            { "instrument_clarinet.png", BinaryData::instrument_clarinet_png, BinaryData::instrument_clarinet_pngSize },
            { "instrument_bass_clarinet.png", BinaryData::instrument_bass_clarinet_png, BinaryData::instrument_bass_clarinet_pngSize },
            { "instrument_oboe.png", BinaryData::instrument_oboe_png, BinaryData::instrument_oboe_pngSize },
            { "instrument_english_horn.png", BinaryData::instrument_english_horn_png, BinaryData::instrument_english_horn_pngSize },
            { "instrument_bassoon.png", BinaryData::instrument_bassoon_png, BinaryData::instrument_bassoon_pngSize },
            { "instrument_contrabassoon.png", BinaryData::instrument_contrabassoon_png, BinaryData::instrument_contrabassoon_pngSize },
            { "instrument_violin.png", BinaryData::instrument_violin_png, BinaryData::instrument_violin_pngSize },
            { "instrument_viola.png", BinaryData::instrument_viola_png, BinaryData::instrument_viola_pngSize },
            { "instrument_cello.png", BinaryData::instrument_cello_png, BinaryData::instrument_cello_pngSize },
            { "instrument_double_bass.png", BinaryData::instrument_double_bass_png, BinaryData::instrument_double_bass_pngSize }
        };
        for (const auto& image : images)
            if (requested.endsWith(image.file))
            {
                data = image.data; size = image.size; mime = "image/png";
                break;
            }
        if (data == nullptr) return std::nullopt;
    }

    std::vector<std::byte> bytes(static_cast<size_t>(size));
    std::memcpy(bytes.data(), data, static_cast<size_t>(size));
    return juce::WebBrowserComponent::Resource { std::move(bytes), mime };
}

juce::Rectangle<int> MainComponent::getContentBounds() const
{
    return getLocalBounds().withTrimmedLeft(238).reduced(28, 22);
}

void MainComponent::showPage(Page page)
{
    currentPage = page;
    const juce::String names[] { utf8("开始演奏"), utf8("音色方案"), utf8("音源与音效"),
                                utf8("电吹管设置"), utf8("声音设置"), utf8("软件设置") };
    title.setText(utf8("风吟 · ") + names[static_cast<int>(page)], juce::dontSendNotification);
    updatePageVisibility();
    resized();
    repaint();
}

void MainComponent::updatePageVisibility()
{
    const auto play = currentPage == Page::play;
    const auto sounds = currentPage == Page::sounds;
    const auto chain = currentPage == Page::chain;
    const auto wind = currentPage == Page::wind;
    const auto audioPage = currentPage == Page::audio;
    const auto software = currentPage == Page::settings;

    for (auto* component : std::array<juce::Component*, 8> { &videoPlayer, &noteLabel, &breathLabel, &recordButton,
                             &recordingStatus, &masterVolume, &masterVolumeLabel, &recordingManagerButton })
        component->setVisible(play);
    for (auto* component : std::array<juce::Component*, 6> { &presetSelector, &savePresetButton, &loadPresetButton,
                             &favoritePresetButton, &defaultPresetButton, &deletePresetButton })
        component->setVisible(sounds);
    for (auto* component : std::array<juce::Component*, 10> { &scanPluginsButton, &pluginSelector, &loadPluginButton,
                             &pluginEditorButton, &effectSelector, &loadEffectButton, &removeEffectButton,
                             &effectEditorButton, &bypassEffectButton, &effectStatus })
        component->setVisible(chain);
    pluginStatus.setVisible(chain || sounds);
    detectButton.setVisible(wind);
    expressionButton.setVisible(wind);
    settingsButton.setVisible(audioPage);
    themeSelector.setVisible(software);
    lowPerformanceToggle.setVisible(software);
    helpButton.setVisible(software);
    licenseButton.setVisible(software);

    const std::array<std::pair<juce::TextButton*, Page>, 6> navigation {{
        { &playNav, Page::play }, { &soundsNav, Page::sounds }, { &chainNav, Page::chain },
        { &windNav, Page::wind }, { &audioNav, Page::audio }, { &softwareNav, Page::settings }
    }};
    for (const auto& [button, navPage] : navigation)
    {
        const auto selected = navPage == currentPage;
        button->setColour(juce::TextButton::buttonColourId,
                          selected ? themeAccent.withAlpha(0.22f) : themeBackground.darker(0.15f));
        button->setColour(juce::TextButton::textColourOffId, selected ? juce::Colours::white : themeMuted);
    }
}

void MainComponent::applyTheme(Theme theme)
{
    currentTheme = theme;
    if (theme == Theme::gold)
    {
        themeBackground = juce::Colour(0xff160e08); themePanel = juce::Colour(0xff28180d);
        themePanelBright = juce::Colour(0xff372210); themeAccent = juce::Colour(0xffffc85a);
        themeAccent2 = juce::Colour(0xffe76e32); themeMuted = juce::Colour(0xffc5a982);
    }
    else if (theme == Theme::minimal)
    {
        themeBackground = juce::Colour(0xff11151b); themePanel = juce::Colour(0xff1b2028);
        themePanelBright = juce::Colour(0xff1f262f); themeAccent = juce::Colour(0xff75a9dc);
        themeAccent2 = juce::Colour(0xff8b98a8); themeMuted = juce::Colour(0xff9caab8);
    }
    else
    {
        themeBackground = juce::Colour(0xff07101d); themePanel = juce::Colour(0xff102238);
        themePanelBright = juce::Colour(0xff142b46); themeAccent = juce::Colour(0xff43d9ff);
        themeAccent2 = juce::Colour(0xff9b63ff); themeMuted = juce::Colour(0xff8fa7bd);
    }
    title.setColour(juce::Label::textColourId, juce::Colours::white);
    deviceStatus.setColour(juce::Label::textColourId, themeMuted);
    audioStatus.setColour(juce::Label::textColourId, themeMuted);
    breathLabel.setColour(juce::Label::textColourId, themeAccent);
    updatePageVisibility();
    repaint();
}

void MainComponent::paint(juce::Graphics& g)
{
    g.fillAll(themeBackground);
    g.setColour(themeBackground.darker(0.3f));
    g.fillRect(getLocalBounds().removeFromLeft(238));
    g.setGradientFill({ themeAccent, 26.0f, 26.0f, themeAccent2, 76.0f, 76.0f, false });
    g.fillRoundedRectangle(26.0f, 24.0f, 48.0f, 48.0f, 14.0f);
    g.setColour(juce::Colours::white);
    g.setFont(juce::FontOptions(24.0f, juce::Font::bold));
    g.drawText(utf8("风"), 26, 24, 48, 48, juce::Justification::centred);
    g.drawText(utf8("风吟"), 86, 24, 120, 28, juce::Justification::centredLeft);
    g.setColour(themeMuted);
    g.setFont(juce::FontOptions(12.0f));
    g.drawText(utf8("电吹管演奏工作站"), 86, 50, 135, 22, juce::Justification::centredLeft);

    if (currentPage != Page::play)
    {
        auto pagePanel = getContentBounds().withTrimmedTop(80).toFloat();
        g.setColour(themePanel);
        g.fillRoundedRectangle(pagePanel, 22.0f);
        g.setColour(themeMuted);
        g.setFont(juce::FontOptions(16.0f));
        const juce::String descriptions[] { {}, utf8("收藏、命名并快速恢复完整的演奏音色。"),
            utf8("按从左到右的顺序管理音源、效果器和最终输出。"),
            utf8("连接电吹管并调整气息、起音与弯音手感。"),
            utf8("选择驱动与输出设备，推荐 ASIO、48000 Hz、128 缓冲区。"),
            utf8("选择视觉主题、性能模式、使用向导和永久授权。") };
        g.drawText(descriptions[static_cast<int>(currentPage)], pagePanel.toNearestInt().reduced(28).removeFromTop(36),
                   juce::Justification::centredLeft);
        return;
    }

    auto area = getContentBounds().toFloat();
    auto stage = area.withTrimmedTop(78.0f).withTrimmedBottom(185.0f);
    g.setColour(themePanel);
    g.fillRoundedRectangle(stage, 22.0f);

    stage.removeFromTop(104.0f);
    const auto centre = stage.removeFromRight(350.0f).getCentre();
    const auto breath = juce::jlimit(0.0f, 1.0f, snapshot.breath);
    const auto radius = 105.0f + breath * 18.0f;
    g.setColour(themeAccent.withAlpha(0.18f + breath * 0.35f));
    g.fillEllipse(centre.x - radius, centre.y - radius, radius * 2.0f, radius * 2.0f);
    g.setColour(themeAccent);
    g.drawEllipse(centre.x - radius, centre.y - radius, radius * 2.0f, radius * 2.0f, 2.0f);
    g.setColour(themeAccent2.withAlpha(0.75f));
    g.drawEllipse(centre.x - radius + 17.0f, centre.y - radius + 17.0f,
                  radius * 2.0f - 34.0f, radius * 2.0f - 34.0f, 1.0f);

    auto visual = area.removeFromBottom(160.0f);
    g.setColour(themePanel);
    g.fillRoundedRectangle(visual, 18.0f);
    g.setGradientFill(juce::ColourGradient(themeAccent, visual.getX(), visual.getBottom(),
                                           themeAccent2, visual.getRight(), visual.getY(), false));
    const auto bars = 54;
    auto meterArea = visual.removeFromRight(34.0f).reduced(7.0f, 12.0f);
    const auto meterWidth = 7.0f;
    g.setColour(juce::Colour(0xff203346));
    g.fillRoundedRectangle(meterArea.getX(), meterArea.getY(), meterWidth, meterArea.getHeight(), 3.0f);
    g.fillRoundedRectangle(meterArea.getRight() - meterWidth, meterArea.getY(), meterWidth, meterArea.getHeight(), 3.0f);
    g.setColour(displayedLeftPeak > 0.9f || displayedRightPeak > 0.9f ? juce::Colour(0xffff526b) : themeAccent);
    g.fillRoundedRectangle(meterArea.getX(), meterArea.getBottom() - meterArea.getHeight() * displayedLeftPeak,
                           meterWidth, meterArea.getHeight() * displayedLeftPeak, 3.0f);
    g.fillRoundedRectangle(meterArea.getRight() - meterWidth, meterArea.getBottom() - meterArea.getHeight() * displayedRightPeak,
                           meterWidth, meterArea.getHeight() * displayedRightPeak, 3.0f);
    const auto width = visual.getWidth() / static_cast<float>(bars);
    for (int i = 0; i < bars; ++i)
    {
        const auto simulated = 0.18f + 0.82f * std::abs(std::sin(simulatedPhase + static_cast<float>(i) * 0.24f));
        const auto level = snapshot.deviceConnected ? spectrumLevels[static_cast<size_t>(i)]
                                                    : simulated * (0.35f + snapshot.breath * 0.65f);
        const auto height = 8.0f + level * 105.0f;
        g.fillRoundedRectangle(visual.getX() + static_cast<float>(i) * width + 2.0f,
                               visual.getBottom() - height - 12.0f,
                               juce::jmax(2.0f, width - 5.0f), height, 2.0f);
    }
    if (! lowPerformanceToggle.getToggleState())
    {
        auto lights = visual.removeFromBottom(12.0f).reduced(6.0f, 1.0f);
        constexpr int lightCount = 34;
        const auto lightWidth = lights.getWidth() / static_cast<float>(lightCount);
        const auto leading = static_cast<int>(simulatedPhase * 4.0f) % lightCount;
        for (int i = 0; i < lightCount; ++i)
        {
            const auto distance = (i - leading + lightCount) % lightCount;
            const auto glow = distance < 6 ? 1.0f - static_cast<float>(distance) / 7.0f : 0.12f;
            g.setColour((i % 2 == 0 ? themeAccent : themeAccent2).withAlpha(glow));
            g.fillRoundedRectangle(lights.getX() + static_cast<float>(i) * lightWidth, lights.getY(),
                                   juce::jmax(3.0f, lightWidth - 5.0f), lights.getHeight(), 3.0f);
        }
    }
}

void MainComponent::resized()
{
    if (webInterface != nullptr)
    {
        webInterface->setBounds(getLocalBounds());
        return;
    }
    {
        auto sidebar = getLocalBounds().removeFromLeft(238).reduced(18);
        sidebar.removeFromTop(92);
        for (auto* button : { &playNav, &soundsNav, &chainNav, &windNav, &audioNav, &softwareNav })
        {
            button->setBounds(sidebar.removeFromTop(50).reduced(0, 4));
            sidebar.removeFromTop(3);
        }

        auto content = getContentBounds();
        auto header = content.removeFromTop(72);
        title.setBounds(header.removeFromLeft(365));
        audioStatus.setBounds(header.removeFromRight(250));
        deviceStatus.setBounds(header);
        auto page = content.withTrimmedTop(8);

        if (currentPage == Page::play)
        {
            auto stage = page.withTrimmedBottom(178);
            auto performanceArea = stage.removeFromRight(350);
            videoPlayer.setBounds(stage.reduced(18, 8));
            noteLabel.setBounds(performanceArea.withSizeKeepingCentre(250, 110).translated(0, -14));
            breathLabel.setBounds(performanceArea.withSizeKeepingCentre(330, 38).translated(0, 70));
            auto actions = page.removeFromBottom(48).reduced(12, 3);
            recordButton.setBounds(actions.removeFromLeft(140)); actions.removeFromLeft(8);
            recordingManagerButton.setBounds(actions.removeFromLeft(116)); actions.removeFromLeft(12);
            recordingStatus.setBounds(actions.removeFromLeft(320));
            masterVolume.setBounds(actions.removeFromRight(260));
            masterVolumeLabel.setBounds(actions.removeFromRight(70));
        }
        else
        {
            auto inner = page.reduced(30).withTrimmedTop(62);
            if (currentPage == Page::sounds)
            {
                presetSelector.setBounds(inner.removeFromTop(48)); inner.removeFromTop(16);
                auto row = inner.removeFromTop(46);
                loadPresetButton.setBounds(row.removeFromLeft(130)); row.removeFromLeft(10);
                savePresetButton.setBounds(row.removeFromLeft(160)); row.removeFromLeft(10);
                favoritePresetButton.setBounds(row.removeFromLeft(100)); row.removeFromLeft(10);
                defaultPresetButton.setBounds(row.removeFromLeft(120)); row.removeFromLeft(10);
                deletePresetButton.setBounds(row.removeFromLeft(90));
                pluginStatus.setBounds(inner.removeFromTop(52));
            }
            else if (currentPage == Page::chain)
            {
                auto row = inner.removeFromTop(52);
                scanPluginsButton.setBounds(row.removeFromLeft(130)); row.removeFromLeft(12);
                pluginSelector.setBounds(row.removeFromLeft(380)); row.removeFromLeft(12);
                loadPluginButton.setBounds(row.removeFromLeft(116)); row.removeFromLeft(10);
                pluginEditorButton.setBounds(row.removeFromLeft(116));
                pluginStatus.setBounds(inner.removeFromTop(58)); inner.removeFromTop(26);
                row = inner.removeFromTop(52);
                effectSelector.setBounds(row.removeFromLeft(380)); row.removeFromLeft(12);
                loadEffectButton.setBounds(row.removeFromLeft(112)); row.removeFromLeft(10);
                effectEditorButton.setBounds(row.removeFromLeft(112)); row.removeFromLeft(10);
                bypassEffectButton.setBounds(row.removeFromLeft(92)); row.removeFromLeft(10);
                removeEffectButton.setBounds(row.removeFromLeft(112));
                effectStatus.setBounds(inner.removeFromTop(58));
            }
            else if (currentPage == Page::wind)
            {
                detectButton.setBounds(inner.removeFromTop(56).removeFromLeft(230)); inner.removeFromTop(18);
                expressionButton.setBounds(inner.removeFromTop(56).removeFromLeft(230));
            }
            else if (currentPage == Page::audio)
                settingsButton.setBounds(inner.removeFromTop(56).removeFromLeft(230));
            else if (currentPage == Page::settings)
            {
                themeSelector.setBounds(inner.removeFromTop(48).removeFromLeft(300)); inner.removeFromTop(14);
                lowPerformanceToggle.setBounds(inner.removeFromTop(46).removeFromLeft(300)); inner.removeFromTop(16);
                helpButton.setBounds(inner.removeFromTop(54).removeFromLeft(210)); inner.removeFromTop(14);
                licenseButton.setBounds(inner.removeFromTop(54).removeFromLeft(210));
            }
        }
    }
}

void MainComponent::timerCallback()
{
    if (++midiConnectionPollCounter >= 30)
    {
        midiConnectionPollCounter = 0;
        midi.pollConnection();
    }
    snapshot = midi.getSnapshot();
    if (videoPlaybackActive && ++audioOutputSyncTicks >= 60)
    {
        audioOutputSyncTicks = 0;
        audio.followSystemDefaultOutput();
    }
    if (snapshot.deviceConnected && ! wasMidiConnected)
    {
        // 已知型号先使用档案中的推荐值，同时在用户完成本调识别的第一次吹奏中
        // 静默观察真实连续控制器；可兼容用户在吹管 App 中改过 MIDI 输出的情况。
        midi.beginBreathDetection();
        automaticBreathDetectionActive = true;
        automaticBreathDetectionEndsAtMs = 0.0;
    }
    else if (! snapshot.deviceConnected && wasMidiConnected)
    {
        automaticBreathDetectionActive = false;
        automaticBreathDetectionEndsAtMs = 0.0;
    }
    wasMidiConnected = snapshot.deviceConnected;
    if (automaticBreathDetectionActive && ! midi.isKeyCalibrationPending()
        && automaticBreathDetectionEndsAtMs <= 0.0)
        automaticBreathDetectionEndsAtMs = juce::Time::getMillisecondCounterHiRes() + 1800.0;
    if (automaticBreathDetectionActive && automaticBreathDetectionEndsAtMs > 0.0
        && juce::Time::getMillisecondCounterHiRes() >= automaticBreathDetectionEndsAtMs)
    {
        (void) midi.finishBreathDetection();
        automaticBreathDetectionActive = false;
        automaticBreathDetectionEndsAtMs = 0.0;
    }
    pluginHost.flushTechniqueValues();
    if (activeTechniqueLearn >= 0)
    {
        const auto learned = midi.consumeTechniqueLearnResult();
        if (learned.ready)
        {
            auto mappings = midi.getTechniqueMappings();
            for (int index = mappings.size(); --index >= 0;)
                if (mappings.getReference(index).technique == learned.mapping.technique)
                    mappings.remove(index);
            auto mapping = learned.mapping;
            mapping.toggle = pendingTechniqueToggle;
            mappings.add(mapping);
            midi.setTechniqueMappings(mappings);
            activeTechniqueLearn = -1;
            techniqueLearnEndsAtMs = 0.0;
            if (webInterface != nullptr)
            {
                auto result = std::make_unique<juce::DynamicObject>();
                result->setProperty("success", true);
                result->setProperty("message", utf8("识别成功；保存音色方案后会永久保留"));
                webInterface->emitEventIfBrowserIsVisible("techniqueLearnResult", juce::var(result.release()));
            }
        }
        else if (juce::Time::getMillisecondCounterHiRes() >= techniqueLearnEndsAtMs)
        {
            midi.cancelTechniqueLearn();
            activeTechniqueLearn = -1;
            techniqueLearnEndsAtMs = 0.0;
            if (webInterface != nullptr)
            {
                auto result = std::make_unique<juce::DynamicObject>();
                result->setProperty("success", false);
                result->setProperty("message", utf8("10 秒内没有识别到控制信号，请重试"));
                webInterface->emitEventIfBrowserIsVisible("techniqueLearnResult", juce::var(result.release()));
            }
        }
    }
    if (++lowLatencyMonitorTicks >= 60)
    {
        lowLatencyMonitorTicks = 0;
        // 只在没有吹气且没有录音时安全升一级缓冲，避免正在演奏时突然重启设备。
        if (masterOutput.isSmartOptimisationEnabled() && snapshot.breath < 4 && ! recorder.isRecording()
            && audio.stabiliseAfterXRuns() && webInterface != nullptr)
        {
            const auto message = utf8("检测到连续丢音，已自动提高一级稳定性");
            webInterface->emitEventIfBrowserIsVisible("audioOptimisationResult", message);
            emitAudioSettingsState(true, message);
        }
    }
    const auto currentAudio = audio.getStatus();
    const auto scanProgress = pluginCatalog.getProgress();
    simulatedPhase += 0.09f;
    displayedLeftPeak = juce::jmax(displayedLeftPeak * 0.88f, juce::jlimit(0.0f, 1.0f, masterOutput.getLeftPeak()));
    displayedRightPeak = juce::jmax(displayedRightPeak * 0.88f, juce::jlimit(0.0f, 1.0f, masterOutput.getRightPeak()));
    if (webInterface != nullptr && webInterfaceReady && ++webUpdateCounter >= (videoPlaybackActive ? 6 : 3))
    {
        webUpdateCounter = 0;
        // FFT 只在真正要刷新网页时计算；视频播放期间由 30 次/秒降到 5 次/秒，
        // 把 CPU 时间优先留给 SWAM 与视频解码。
        masterOutput.getSpectrum(spectrumLevels);
        auto state = std::make_unique<juce::DynamicObject>();
        state->setProperty("deviceConnected", snapshot.deviceConnected);
        state->setProperty("deviceName", midi.getConnectedDeviceName());
        const auto deviceProfile = midi.getActiveProfile();
        state->setProperty("deviceProfileName", utf8(deviceProfile.displayName.c_str()));
        state->setProperty("deviceProfileId", utf8(deviceProfile.id.c_str()));
        state->setProperty("deviceRecognized", deviceProfile.id != "generic-wind-controller");
        state->setProperty("breathController", deviceProfile.breathController);
        state->setProperty("safeOnsetProtection", deviceProfile.safeOnsetProtection);
        state->setProperty("hasBiteSensor", deviceProfile.hasBiteSensor);
        state->setProperty("hasThumbController", deviceProfile.hasThumbController);
        state->setProperty("hasAssignableButtons", deviceProfile.hasAssignableButtons);
        state->setProperty("hasMotionController", deviceProfile.hasMotionController);
        state->setProperty("breath", snapshot.breath);
        state->setProperty("note", snapshot.lastNote);
        state->setProperty("noteReceived", snapshot.lastNote >= 0);
        state->setProperty("audioDevice", currentAudio.deviceName);
        state->setProperty("audioDeviceType", currentAudio.deviceType);
        state->setProperty("audioSampleRate", currentAudio.sampleRate);
        state->setProperty("audioBufferSize", currentAudio.bufferSize);
        const auto pluginLatencyMs = currentAudio.sampleRate > 0.0
            ? pluginHost.getProcessingLatencySamples() * 1000.0 / currentAudio.sampleRate : 0.0;
        state->setProperty("latency", currentAudio.estimatedBufferLatencyMs + pluginLatencyMs);
        state->setProperty("pluginLatency", pluginLatencyMs);
        state->setProperty("audioCpu", currentAudio.cpuUsage);
        state->setProperty("audioXruns", currentAudio.xRunCount);
        state->setProperty("activated", isActivated);
        state->setProperty("machineCode", machineCode);
        state->setProperty("recording", recorder.isRecording());
        state->setProperty("transposeSemitones", midi.getTransposeSemitones());
        state->setProperty("sourceKey", midi.getSourceKey());
        state->setProperty("targetKey", midi.getTargetKey());
        state->setProperty("keyCalibrationPending", midi.isKeyCalibrationPending());
        state->setProperty("keyCalibrated", midi.isKeyCalibrated());
        state->setProperty("automaticBreathDetection", automaticBreathDetectionActive);
        state->setProperty("reverbMix", masterOutput.getReverbMix());
        state->setProperty("eqTone", masterOutput.getEqTone());
        state->setProperty("limiterCeiling", masterOutput.getLimiterCeiling());
        state->setProperty("smartOptimisation", masterOutput.isSmartOptimisationEnabled());
        const auto currentPluginName = pluginHost.hasPlugin() ? pluginHost.getPluginName() : utf8("SWAM Soprano Sax");
        state->setProperty("pluginName", pluginHost.hasPlugin() ? currentPluginName : utf8("安全测试音源"));
        state->setProperty("pluginLoaded", pluginHost.hasPlugin());
        state->setProperty("pluginLoading", pluginLoading);
        state->setProperty("instrumentChineseName", utf8(fengyin::SwamPluginClassifier::instrumentChineseName(currentPluginName.toStdString())));
        state->setProperty("instrumentKey", utf8(fengyin::SwamPluginClassifier::instrumentKey(currentPluginName.toStdString())));
        state->setProperty("scanning", scanProgress.scanning);
        state->setProperty("scanProgress", scanProgress.fraction);
        state->setProperty("pluginStatus", pluginStatus.getText());
        state->setProperty("effectName", pluginHost.hasEffect() ? pluginHost.getEffectName() : juce::String());
        state->setProperty("effectLoaded", pluginHost.hasEffect());
        state->setProperty("effectLoading", effectLoading);
        state->setProperty("presetEditing", editingPresetId.isNotEmpty());
        state->setProperty("techniqueLearning", activeTechniqueLearn);
        const auto currentFamily = pluginHost.hasPlugin()
            ? fengyin::SwamPluginClassifier::classify(currentPluginName.toStdString(), {})
            : fengyin::SwamFamily::notSwam;
        state->setProperty("instrumentFamily", utf8(fengyin::SwamPluginClassifier::familyChineseName(currentFamily)));
        juce::Array<juce::var> techniqueMappings;
        for (int technique = 0; technique < static_cast<int>(fengyin::PerformanceTechnique::count); ++technique)
        {
            const auto target = static_cast<fengyin::PerformanceTechnique>(technique);
            const auto advice = fengyin::TechniqueAdvisor::advise(deviceProfile, currentFamily, target);
            auto item = std::make_unique<juce::DynamicObject>();
            item->setProperty("technique", technique);
            item->setProperty("relevant", advice.relevantToInstrument);
            item->setProperty("pluginSupported", pluginHost.supportsTechnique(target));
            item->setProperty("hardwareAvailable", advice.hardwareAvailable);
            item->setProperty("supported", advice.relevantToInstrument && pluginHost.supportsTechnique(target));
            item->setProperty("recommendedSource", utf8(advice.recommendedSource));
            item->setProperty("recommendationReason", utf8(advice.reason));
            item->setProperty("sourceType", 0);
            item->setProperty("sourceNumber", -1);
            item->setProperty("toggle", false);
            for (const auto& mapping : midi.getTechniqueMappings())
                if (static_cast<int>(mapping.technique) == technique)
                {
                    item->setProperty("sourceType", static_cast<int>(mapping.sourceType));
                    item->setProperty("sourceNumber", mapping.sourceNumber);
                    item->setProperty("toggle", mapping.toggle);
                    break;
                }
            techniqueMappings.add(juce::var(item.release()));
        }
        state->setProperty("techniqueMappings", juce::var(techniqueMappings));
        juce::Array<juce::var> instruments;
        for (const auto& plugin : cachedInstrumentPlugins)
        {
            auto item = std::make_unique<juce::DynamicObject>();
            item->setProperty("name", plugin.name);
            item->setProperty("chineseName", utf8(fengyin::SwamPluginClassifier::instrumentChineseName(plugin.name.toStdString())));
            item->setProperty("instrumentKey", utf8(fengyin::SwamPluginClassifier::instrumentKey(plugin.name.toStdString())));
            item->setProperty("label", utf8(fengyin::SwamPluginClassifier::instrumentChineseName(plugin.name.toStdString()))
                                       + utf8(" · ") + plugin.name);
            instruments.add(juce::var(item.release()));
        }
        juce::Array<juce::var> effects;
        for (const auto& effect : cachedEffectPlugins) effects.add(effect.name);
        state->setProperty("instruments", juce::var(instruments));
        state->setProperty("effects", juce::var(effects));
        juce::Array<juce::var> presets;
        for (const auto& preset : cachedPresets)
        {
            auto item = std::make_unique<juce::DynamicObject>();
            item->setProperty("name", preset.name);
            item->setProperty("favorite", preset.favorite);
            presets.add(juce::var(item.release()));
        }
        state->setProperty("presets", juce::var(presets));
        webInterface->emitEventIfBrowserIsVisible("backendState", juce::var(state.release()));
    }
    if (! autoGuideShown && autoGuideAtMs > 0.0 && juce::Time::getMillisecondCounterHiRes() >= autoGuideAtMs)
    {
        autoGuideShown = true;
        if (! getOnboardingMarkerFile().existsAsFile()) showSetupGuide(true);
    }

    if (currentAudio.ready)
    {
        audioStatus.setText(currentAudio.deviceName + " · "
                                + juce::String(currentAudio.estimatedBufferLatencyMs, 1) + " ms",
                            juce::dontSendNotification);
    }
    else
    {
        audioStatus.setText(utf8("音频设备未就绪"), juce::dontSendNotification);
    }

    if (scanProgress.scanning)
    {
        pluginStatus.setText(utf8("正在扫描音源：")
                                + juce::String(juce::roundToInt(scanProgress.fraction * 100.0f)) + "%",
                             juce::dontSendNotification);
        scanPluginsButton.setEnabled(false);
        loadPluginButton.setEnabled(false);
    }
    else
    {
        scanPluginsButton.setEnabled(isActivated);
        loadPluginButton.setEnabled(isActivated && pluginSelector.getSelectedId() > 0);
        if (! pluginChoicesLoaded && scanProgress.fraction >= 1.0f)
            refreshPluginChoices();
    }
    savePresetButton.setEnabled(isActivated && pluginHost.hasPlugin());
    loadPresetButton.setEnabled(isActivated && presetSelector.getSelectedId() > 0);
    favoritePresetButton.setEnabled(isActivated && presetSelector.getSelectedId() > 0);
    defaultPresetButton.setEnabled(isActivated && presetSelector.getSelectedId() > 0);
    deletePresetButton.setEnabled(isActivated && presetSelector.getSelectedId() > 0);
    loadEffectButton.setEnabled(isActivated && pluginHost.hasPlugin() && effectSelector.getSelectedId() > 0);
    removeEffectButton.setEnabled(isActivated && pluginHost.hasEffect());
    pluginEditorButton.setEnabled(isActivated && pluginHost.hasPlugin());
    effectEditorButton.setEnabled(isActivated && pluginHost.hasEffect());
    bypassEffectButton.setEnabled(isActivated && pluginHost.hasEffect());
    recordButton.setEnabled(isActivated || recorder.isRecording());
    if (recorder.isRecording())
    {
        const auto seconds = juce::roundToInt(recorder.getElapsedSeconds());
        recordingStatus.setText(utf8("正在录音  ") + juce::String::formatted("%02d:%02d", seconds / 60, seconds % 60),
                                juce::dontSendNotification);
    }
    if (breathDetectionActive)
    {
        const auto remaining = juce::jmax(0, juce::roundToInt((breathDetectionEndsAtMs - juce::Time::getMillisecondCounterHiRes()) / 1000.0));
        deviceStatus.setText(utf8("请持续吹奏一个长音…还剩 ") + juce::String(remaining) + utf8(" 秒"), juce::dontSendNotification);
        if (juce::Time::getMillisecondCounterHiRes() >= breathDetectionEndsAtMs)
        {
            breathDetectionActive = false;
            const auto controller = midi.finishBreathDetection();
            juce::AlertWindow::showMessageBoxAsync(controller >= 0 ? juce::MessageBoxIconType::InfoIcon
                                                                    : juce::MessageBoxIconType::WarningIcon,
                controller >= 0 ? utf8("识别成功") : utf8("没有识别到气息"),
                controller >= 0 ? utf8("已自动设置气息控制器 CC") + juce::String(controller)
                                : utf8("请确认电吹管已经连接，再吹一个音量由弱到强的长音。"));
        }
    }
    else if (snapshot.deviceConnected)
    {
        deviceStatus.setText(utf8("● ") + midi.getConnectedDeviceName() + utf8(" · MIDI 信号已连接"),
                             juce::dontSendNotification);
        noteLabel.setText(snapshot.lastNote >= 0 ? midiNoteName(snapshot.lastNote) : "—",
                          juce::dontSendNotification);
        breathLabel.setText(utf8("气息强度 ") + juce::String(juce::roundToInt(snapshot.breath * 100.0f)) + "%",
                            juce::dontSendNotification);
    }
    else
    {
        const auto simulatedBreath = 0.52f + 0.25f * std::sin(simulatedPhase * 0.55f);
        snapshot.breath = simulatedBreath;
        deviceStatus.setText(utf8("○ 未检测到电吹管 · 当前显示模拟数据"), juce::dontSendNotification);
        noteLabel.setText("5\nG4", juce::dontSendNotification);
        breathLabel.setText(utf8("模拟气息 ") + juce::String(juce::roundToInt(simulatedBreath * 100.0f)) + "%",
                            juce::dontSendNotification);
    }
    repaint();
}

void MainComponent::startPluginScan()
{
    pluginChoicesLoaded = false;
    pluginSelector.clear(juce::dontSendNotification);
    pluginCatalog.startScan(pluginCatalog.getRecommendedVst3Paths(), false);
    pluginStatus.setText(utf8("正在准备扫描……"), juce::dontSendNotification);
}

void MainComponent::refreshPluginChoices()
{
    pluginSelector.clear(juce::dontSendNotification);
    effectSelector.clear(juce::dontSendNotification);
    cachedInstrumentPlugins.clear();
    cachedEffectPlugins.clear();
    const auto plugins = pluginCatalog.getPlugins();
    const auto swamPlugins = pluginCatalog.getSwamPlugins();
    int itemId = 1;
    const fengyin::SwamFamily familyOrder[] { fengyin::SwamFamily::saxophone, fengyin::SwamFamily::brass,
        fengyin::SwamFamily::woodwind, fengyin::SwamFamily::strings, fengyin::SwamFamily::other };
    for (const auto family : familyOrder)
    {
        bool headingAdded = false;
        for (const auto& plugin : swamPlugins)
            if (plugin.isInstrument
                && fengyin::SwamPluginClassifier::classify(plugin.name.toStdString(), plugin.manufacturerName.toStdString()) == family)
            {
                if (! headingAdded)
                {
                    pluginSelector.addSectionHeading(utf8(fengyin::SwamPluginClassifier::familyChineseName(family)));
                    headingAdded = true;
                }
                cachedInstrumentPlugins.add(plugin);
                pluginSelector.addItem(utf8(fengyin::SwamPluginClassifier::instrumentChineseName(plugin.name.toStdString()))
                                       + utf8(" · ") + plugin.name, itemId++);
            }
    }
    bool otherHeadingAdded = false;
    for (const auto& plugin : plugins)
    {
        bool isSwam = false;
        for (const auto& swam : swamPlugins)
            if (swam.createIdentifierString() == plugin.createIdentifierString())
                isSwam = true;
        if (! isSwam && plugin.isInstrument)
        {
            if (! otherHeadingAdded)
            {
                pluginSelector.addSectionHeading(utf8("其他 VST3 音源"));
                otherHeadingAdded = true;
            }
            cachedInstrumentPlugins.add(plugin);
            pluginSelector.addItem(plugin.name, itemId++);
        }
        if (! plugin.isInstrument)
        {
            cachedEffectPlugins.add(plugin);
            effectSelector.addItem(plugin.name, cachedEffectPlugins.size());
        }
    }
    pluginChoicesLoaded = true;
    pluginStatus.setText(utf8("扫描完成，共找到 ") + juce::String(plugins.size()) + utf8(" 个音源或效果器"),
                         juce::dontSendNotification);
    tryAutoLoadDefaultPreset();
}

void MainComponent::loadSelectedPlugin()
{
    const auto selectedIndex = pluginSelector.getSelectedId() - 1;
    if (! juce::isPositiveAndBelow(selectedIndex, cachedInstrumentPlugins.size()))
    {
        pluginStatus.setText(utf8("请先选择一个音源"), juce::dontSendNotification);
        return;
    }

    const auto chosen = cachedInstrumentPlugins.getReference(selectedIndex);

    const auto status = audio.getStatus();
    pluginStatus.setText(utf8("正在加载：") + chosen.name, juce::dontSendNotification);
    pluginLoading = true;
    loadPluginButton.setEnabled(false);
    pluginHost.loadAsync(chosen,
                         status.sampleRate > 0.0 ? status.sampleRate : 48000.0,
                         status.bufferSize > 0 ? status.bufferSize : 128,
                         [this](bool success, const juce::String& message)
                         {
                             pluginLoading = false;
                             loadPluginButton.setEnabled(true);
                             if (! success)
                             {
                                 useTestSynth();
                                 pluginStatus.setText(utf8("加载失败，已恢复测试音源：") + message,
                                                      juce::dontSendNotification);
                                 if (webInterface != nullptr)
                                 {
                                     auto result = std::make_unique<juce::DynamicObject>();
                                     result->setProperty("success", false);
                                     result->setProperty("message", message);
                                     webInterface->emitEventIfBrowserIsVisible("pluginLoadResult", juce::var(result.release()));
                                 }
                                 return;
                             }
                             activatePluginOutput(message);
                             if (webInterface != nullptr)
                             {
                                 auto result = std::make_unique<juce::DynamicObject>();
                                 result->setProperty("success", true);
                                 result->setProperty("message", message);
                                 webInterface->emitEventIfBrowserIsVisible("pluginLoadResult", juce::var(result.release()));
                             }
                         });
}

void MainComponent::loadSelectedEffect()
{
    const auto index = effectSelector.getSelectedId() - 1;
    if (! juce::isPositiveAndBelow(index, cachedEffectPlugins.size()) || ! pluginHost.hasPlugin()) return;
    const auto chosen = cachedEffectPlugins.getReference(index);
    const auto status = audio.getStatus();
    effectStatus.setText(utf8("正在加载效果器：") + chosen.name, juce::dontSendNotification);
    effectLoading = true;
    loadEffectButton.setEnabled(false);
    pluginHost.loadEffectAsync(chosen, status.sampleRate > 0.0 ? status.sampleRate : 48000.0,
        status.bufferSize > 0 ? status.bufferSize : 128,
        [this](bool success, const juce::String& message)
        {
            effectLoading = false;
            loadEffectButton.setEnabled(true);
            if (success)
            {
                bypassEffectButton.setToggleState(false, juce::dontSendNotification);
                bypassEffectButton.setButtonText(utf8("旁通"));
            }
            effectStatus.setText(success ? utf8("当前效果器：") + message : utf8("效果器加载失败：") + message,
                                 juce::dontSendNotification);
        });
}

void MainComponent::removeEffect()
{
    effectLoading = false;
    pluginHost.unloadEffect();
    bypassEffectButton.setToggleState(false, juce::dontSendNotification);
    bypassEffectButton.setButtonText(utf8("旁通"));
    effectStatus.setText(utf8("效果器：未使用"), juce::dontSendNotification);
}

void MainComponent::useTestSynth()
{
    midi.setPerformanceSink(nullptr);
    pluginHost.detach();
    pluginHost.unload();
    audio.getDeviceManager().removeAudioCallback(&testSynth);
    audio.getDeviceManager().addAudioCallback(&testSynth);
    midi.setPerformanceSink(&testSynth);
    masterOutput.setInstrumentProfile(fengyin::InstrumentMixProfile::generic);
}

void MainComponent::refreshPresetChoices()
{
    cachedPresets = presetStore.loadAll();
    std::sort(cachedPresets.begin(), cachedPresets.end(), [](const auto& a, const auto& b)
    { return a.favorite != b.favorite ? a.favorite > b.favorite : a.name.compareIgnoreCase(b.name) < 0; });
    presetSelector.clear(juce::dontSendNotification);
    for (int i = 0; i < cachedPresets.size(); ++i)
    {
        const auto& preset = cachedPresets.getReference(i);
        const auto isDefault = preset.id == presetStore.getDefaultId();
        presetSelector.addItem((preset.favorite ? utf8("★ ") : juce::String()) + preset.name
                               + (isDefault ? utf8(" · 默认") : juce::String()), i + 1);
    }
}

void MainComponent::saveCurrentPreset()
{
    if (! pluginHost.hasPlugin())
    {
        pluginStatus.setText(utf8("请先加载一个 VST3 音源"), juce::dontSendNotification);
        return;
    }

    juce::String initialName = utf8(fengyin::SwamPluginClassifier::instrumentChineseName(pluginHost.getPluginName().toStdString()));
    for (const auto& existing : cachedPresets)
        if (existing.id == editingPresetId) initialName = existing.name;
    const auto isEditing = editingPresetId.isNotEmpty();
    savePresetDialog = std::make_unique<juce::AlertWindow>(isEditing ? utf8("保存方案修改") : utf8("保存音色方案"),
                                                           isEditing ? utf8("修改名称或直接保存，将覆盖原来的方案。")
                                                                     : utf8("给这套音源和效果器起一个容易记住的名字。"),
                                                           juce::MessageBoxIconType::QuestionIcon);
    savePresetDialog->addTextEditor("name",
        initialName,
        utf8("方案名称"));
    savePresetDialog->addButton(utf8("保存"), 1, juce::KeyPress(juce::KeyPress::returnKey));
    savePresetDialog->addButton(utf8("取消"), 0, juce::KeyPress(juce::KeyPress::escapeKey));
    savePresetDialog->enterModalState(true, juce::ModalCallbackFunction::create([this](int result)
    {
        if (result == 1 && savePresetDialog != nullptr)
            commitCurrentPreset(savePresetDialog->getTextEditorContents("name").trim());
        else
            editingPresetId.clear();
        savePresetDialog.reset();
    }), false);
}

void MainComponent::commitCurrentPreset(const juce::String& name)
{
    if (name.isEmpty()) return;
    fengyin::SoundPreset preset;
    if (editingPresetId.isNotEmpty())
        for (const auto& existing : cachedPresets)
            if (existing.id == editingPresetId) { preset = existing; break; }
    preset.id = editingPresetId.isNotEmpty() ? editingPresetId : juce::Uuid().toString();
    preset.name = name;
    preset.pluginIdentifier = pluginHost.getPluginIdentifier();
    preset.pluginState = pluginHost.savePluginState();
    preset.effectIdentifier = pluginHost.getEffectIdentifier();
    preset.effectState = pluginHost.saveEffectState();
    preset.effectBypassed = pluginHost.isEffectBypassed();
    preset.eqTone = masterOutput.getEqTone();
    preset.reverbMix = masterOutput.getReverbMix();
    // 设备手感和技巧映射独立按设备保存，不进入音色方案。
    preset.breathController = 2;
    preset.breathCurve = 0.9f;
    preset.breathSmoothing = 0.28f;
    preset.techniqueMappings.clear();

    if (presetStore.save(preset))
    {
        const auto savedId = preset.id;
        editingPresetId.clear();
        refreshPresetChoices();
        for (int i = 0; i < cachedPresets.size(); ++i)
            if (cachedPresets.getReference(i).id == savedId)
                presetSelector.setSelectedId(i + 1, juce::dontSendNotification);
        pluginStatus.setText(utf8("音色方案已保存或更新"), juce::dontSendNotification);
    }
    else
    {
        pluginStatus.setText(utf8("保存失败，请检查磁盘空间"), juce::dontSendNotification);
    }
}

void MainComponent::togglePresetFavorite()
{
    const auto index = presetSelector.getSelectedId() - 1;
    if (! juce::isPositiveAndBelow(index, cachedPresets.size())) return;
    const auto preset = cachedPresets.getReference(index);
    if (presetStore.setFavorite(preset.id, ! preset.favorite))
    {
        refreshPresetChoices();
        for (int i = 0; i < cachedPresets.size(); ++i)
            if (cachedPresets.getReference(i).id == preset.id)
                presetSelector.setSelectedId(i + 1, juce::dontSendNotification);
    }
}

void MainComponent::makePresetDefault()
{
    const auto index = presetSelector.getSelectedId() - 1;
    if (! juce::isPositiveAndBelow(index, cachedPresets.size())) return;
    if (presetStore.setDefaultId(cachedPresets.getReference(index).id))
    {
        refreshPresetChoices();
        pluginStatus.setText(utf8("已设为启动后的默认音色"), juce::dontSendNotification);
    }
}

void MainComponent::deleteSelectedPreset()
{
    const auto index = presetSelector.getSelectedId() - 1;
    if (! juce::isPositiveAndBelow(index, cachedPresets.size())) return;
    const auto preset = cachedPresets.getReference(index);
    auto safe = juce::Component::SafePointer<MainComponent>(this);
    juce::AlertWindow::showOkCancelBox(juce::MessageBoxIconType::WarningIcon, utf8("删除音色方案"),
        utf8("确定删除“") + preset.name + utf8("”吗？此操作无法撤销。"), utf8("确认删除"), utf8("取消"), this,
        juce::ModalCallbackFunction::create([safe, id = preset.id](int result)
        {
            if (result == 1 && safe != nullptr && safe->presetStore.remove(id))
            {
                if (safe->editingPresetId == id) safe->editingPresetId.clear();
                safe->refreshPresetChoices();
                safe->pluginStatus.setText(utf8("音色方案已删除"), juce::dontSendNotification);
            }
        }));
}

void MainComponent::tryAutoLoadDefaultPreset()
{
    if (attemptedDefaultPreset || ! isActivated || pluginCatalog.getPlugins().isEmpty()) return;
    const auto defaultId = presetStore.getDefaultId();
    if (defaultId.isEmpty()) return;
    attemptedDefaultPreset = true;
    for (int i = 0; i < cachedPresets.size(); ++i)
        if (cachedPresets.getReference(i).id == defaultId)
        {
            presetSelector.setSelectedId(i + 1, juce::dontSendNotification);
            loadSelectedPreset();
            return;
        }
}

void MainComponent::showRecordingManager()
{
    managedRecordings = fengyin::RecordingService::getRecordings();
    if (managedRecordings.isEmpty())
    {
        juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::InfoIcon, utf8("还没有录音"),
                                               utf8("点击“开始录音”，完成后录音会自动出现在这里。"));
        return;
    }
    juce::StringArray choices;
    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    for (const auto& file : managedRecordings)
    {
        double seconds = 0.0;
        if (auto reader = std::unique_ptr<juce::AudioFormatReader>(formats.createReaderFor(file)))
            seconds = static_cast<double>(reader->lengthInSamples) / reader->sampleRate;
        const auto sizeMb = static_cast<double>(file.getSize()) / (1024.0 * 1024.0);
        choices.add(file.getFileNameWithoutExtension() + juce::String::formatted("  ·  %02d:%02d  ·  %.1f MB",
                    static_cast<int>(seconds) / 60, static_cast<int>(seconds) % 60, sizeMb));
    }
    recordingManagerDialog = std::make_unique<juce::AlertWindow>(utf8("我的录音"), utf8("录音按时间从新到旧排列。删除会移入系统回收站。"),
                                                                  juce::MessageBoxIconType::QuestionIcon);
    recordingManagerDialog->addComboBox("recording", choices, utf8("选择录音"));
    recordingManagerDialog->getComboBoxComponent("recording")->setSelectedId(1, juce::dontSendNotification);
    recordingManagerDialog->addButton(utf8("打开文件夹"), 1);
    recordingManagerDialog->addButton(utf8("删除到回收站"), 2);
    recordingManagerDialog->addButton(utf8("关闭"), 0, juce::KeyPress(juce::KeyPress::escapeKey));
    recordingManagerDialog->enterModalState(true, juce::ModalCallbackFunction::create([this](int result)
    {
        const auto index = recordingManagerDialog != nullptr
            ? recordingManagerDialog->getComboBoxComponent("recording")->getSelectedId() - 1 : -1;
        if (result == 1)
            fengyin::RecordingService::getRecordingsFolder().revealToUser();
        else if (result == 2 && juce::isPositiveAndBelow(index, managedRecordings.size()))
        {
            const auto file = managedRecordings.getReference(index);
            if (file.moveToTrash())
                recordingStatus.setText(utf8("已移到回收站：") + file.getFileName(), juce::dontSendNotification);
            else
                recordingStatus.setText(utf8("无法删除录音，请关闭正在使用此文件的程序"), juce::dontSendNotification);
        }
        recordingManagerDialog.reset();
    }), false);
}

juce::File MainComponent::getOnboardingMarkerFile()
{
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("FengYin").getChildFile("setup-complete.txt");
}

void MainComponent::showSetupGuide(bool automatic)
{
    if (setupGuideDialog != nullptr) return;
    const auto licenseReady = license.getStatus().activated;
    const auto midiReady = midi.getSnapshot().deviceConnected;
    const auto audioReady = audio.getStatus().ready;
    const auto pluginsReady = ! pluginCatalog.getPlugins().isEmpty();
    int nextStep = ! licenseReady ? 1 : ! midiReady ? 2 : ! audioReady ? 3 : ! pluginsReady ? 4 : 5;
    const auto mark = [](bool ready) { return ready ? juce::String::fromUTF8("✓ 已完成") : juce::String::fromUTF8("○ 待完成"); };
    auto message = utf8("1. 软件激活：") + mark(licenseReady) + "\n"
                 + utf8("2. 电吹管连接：") + mark(midiReady) + "\n"
                 + utf8("3. 声音设备：") + mark(audioReady) + "\n"
                 + utf8("4. SWAM 音源：") + mark(pluginsReady) + "\n\n";
    const juce::String instructions[] {
        {}, utf8("先完成永久激活。软件会显示本机码，收到激活码后粘贴一次即可。"),
        utf8("请连接并打开电吹管，然后运行连接向导。"),
        utf8("请检查声音设备。推荐 ASIO、48000 Hz、缓冲区 128。"),
        utf8("最后扫描电脑中的 SWAM/VST3 音源。首次扫描可能需要一些时间。"),
        utf8("基础设置已经完成，可以开始演奏。")
    };
    message += instructions[nextStep];
    setupGuideDialog = std::make_unique<juce::AlertWindow>(automatic ? utf8("欢迎使用风吟") : utf8("风吟使用向导"), message,
                                                            nextStep == 5 ? juce::MessageBoxIconType::InfoIcon
                                                                          : juce::MessageBoxIconType::QuestionIcon);
    const juce::String buttonLabels[] { {}, utf8("去激活"), utf8("连接电吹管"), utf8("声音设置"), utf8("扫描音源"), utf8("完成") };
    setupGuideDialog->addButton(buttonLabels[nextStep], 1, juce::KeyPress(juce::KeyPress::returnKey));
    setupGuideDialog->addButton(automatic ? utf8("稍后再说") : utf8("关闭"), 0, juce::KeyPress(juce::KeyPress::escapeKey));
    setupGuideDialog->enterModalState(true, juce::ModalCallbackFunction::create([this, nextStep](int result)
    {
        setupGuideDialog.reset();
        if (result != 1) return;
        if (nextStep == 1) showActivationDialog();
        else if (nextStep == 2) showMidiSetup();
        else if (nextStep == 3) showDeviceSettings();
        else if (nextStep == 4) startPluginScan();
        else
        {
            const auto marker = getOnboardingMarkerFile();
            marker.getParentDirectory().createDirectory();
            marker.replaceWithText("complete");
        }
    }), false);
}

void MainComponent::loadSelectedPreset(std::function<void(bool, const juce::String&)> completion)
{
    const auto index = presetSelector.getSelectedId() - 1;
    if (! juce::isPositiveAndBelow(index, cachedPresets.size()))
    {
        if (completion) completion(false, utf8("请选择一个音色方案"));
        return;
    }
    const auto preset = cachedPresets.getReference(index);

    juce::PluginDescription chosen;
    bool found = false;
    for (const auto& candidate : pluginCatalog.getPlugins())
    {
        if (candidate.createIdentifierString() == preset.pluginIdentifier)
        {
            chosen = candidate;
            found = true;
            break;
        }
    }
    if (! found)
    {
        const auto message = utf8("找不到此方案需要的音源，请重新扫描");
        pluginStatus.setText(message, juce::dontSendNotification);
        if (completion) completion(false, message);
        return;
    }

    const auto status = audio.getStatus();
    pluginStatus.setText(utf8("正在恢复音色方案……"), juce::dontSendNotification);
    pluginLoading = true;
    pluginHost.loadAsync(chosen,
                         status.sampleRate > 0.0 ? status.sampleRate : 48000.0,
                         status.bufferSize > 0 ? status.bufferSize : 128,
                         [this, preset, completion](bool success, const juce::String& message)
                         {
                             pluginLoading = false;
                             if (! success)
                             {
                                 useTestSynth();
                                 pluginStatus.setText(utf8("方案载入失败：") + message, juce::dontSendNotification);
                                 if (completion) completion(false, utf8("方案载入失败：") + message);
                                 return;
                             }
                             pluginHost.restorePluginState(preset.pluginState.getData(), preset.pluginState.getSize());
                             masterOutput.setEqTone(preset.eqTone);
                             masterOutput.setReverbMix(preset.reverbMix);
                             activatePluginOutput(message);
                             pluginStatus.setText(utf8("已恢复音色：") + preset.name, juce::dontSendNotification);
                             if (preset.effectIdentifier.isEmpty())
                             {
                                 pluginHost.unloadEffect();
                                 effectStatus.setText(utf8("效果器：未使用"), juce::dontSendNotification);
                                 if (completion) completion(true, utf8("音色方案已载入"));
                                 return;
                             }
                             juce::PluginDescription effect;
                             bool foundEffect = false;
                             for (const auto& candidate : pluginCatalog.getPlugins())
                                 if (candidate.createIdentifierString() == preset.effectIdentifier)
                                 { effect = candidate; foundEffect = true; break; }
                             if (! foundEffect)
                             {
                                 const auto error = utf8("找不到方案中的效果器；音源已恢复，但方案未完整载入");
                                 effectStatus.setText(error, juce::dontSendNotification);
                                 if (completion) completion(false, error);
                                 return;
                             }
                             const auto audioStatusNow = audio.getStatus();
                             effectLoading = true;
                             pluginHost.loadEffectAsync(effect,
                                 audioStatusNow.sampleRate > 0.0 ? audioStatusNow.sampleRate : 48000.0,
                                 audioStatusNow.bufferSize > 0 ? audioStatusNow.bufferSize : 128,
                                 [this, preset, completion](bool effectLoaded, const juce::String& effectMessage)
                                 {
                                     effectLoading = false;
                                     if (effectLoaded)
                                     {
                                         pluginHost.restoreEffectState(preset.effectState.getData(), preset.effectState.getSize());
                                         pluginHost.setEffectBypassed(preset.effectBypassed);
                                         bypassEffectButton.setToggleState(preset.effectBypassed, juce::dontSendNotification);
                                         bypassEffectButton.setButtonText(preset.effectBypassed ? utf8("已旁通") : utf8("旁通"));
                                         effectStatus.setText(utf8("已恢复效果器：") + effectMessage, juce::dontSendNotification);
                                         if (completion) completion(true, utf8("音色方案已完整载入"));
                                     }
                                     else
                                     {
                                         effectStatus.setText(utf8("效果器恢复失败：") + effectMessage, juce::dontSendNotification);
                                         if (completion) completion(false, utf8("效果器恢复失败：") + effectMessage);
                                     }
                                 });
                         });
}

void MainComponent::activatePluginOutput(const juce::String& pluginName)
{
    audio.getDeviceManager().removeAudioCallback(&testSynth);
    pluginHost.attachTo(audio.getDeviceManager());
    const auto family = fengyin::SwamPluginClassifier::classify(pluginName.toStdString(), {});
    midi.setTechniqueContext(fengyin::SwamPluginClassifier::familyKey(family));
    midi.setPerformanceSink(&pluginHost);
    switch (family)
    {
        case fengyin::SwamFamily::saxophone: masterOutput.setInstrumentProfile(fengyin::InstrumentMixProfile::saxophone); break;
        case fengyin::SwamFamily::brass:     masterOutput.setInstrumentProfile(fengyin::InstrumentMixProfile::brass); break;
        case fengyin::SwamFamily::woodwind:  masterOutput.setInstrumentProfile(fengyin::InstrumentMixProfile::woodwind); break;
        case fengyin::SwamFamily::strings:   masterOutput.setInstrumentProfile(fengyin::InstrumentMixProfile::strings); break;
        case fengyin::SwamFamily::notSwam:
        case fengyin::SwamFamily::other:     masterOutput.setInstrumentProfile(fengyin::InstrumentMixProfile::generic); break;
    }
    pluginStatus.setText(utf8("当前音源：") + pluginName, juce::dontSendNotification);
    if (! pluginHost.hasEffect())
        effectStatus.setText(utf8("效果器：未使用"), juce::dontSendNotification);
}

void MainComponent::refreshLicenseUi()
{
    const auto status = license.getStatus();
    isActivated = status.activated;
    licenseButton.setButtonText(status.activated ? utf8("✓ 永久版") : utf8("激活软件"));
    licenseButton.setColour(juce::TextButton::buttonColourId,
                            status.activated ? juce::Colour(0xff176b57) : juce::Colour(0xff7a4d16));
}

void MainComponent::showActivationDialog()
{
    const auto current = license.getStatus();
    if (current.activated)
    {
        juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::InfoIcon,
                                               utf8("风吟已永久激活"),
                                               utf8("授权编号：") + current.licenseId + "\n" + utf8("本机码：") + license.getMachineCode());
        return;
    }

    activationDialog = std::make_unique<juce::AlertWindow>(utf8("激活风吟"),
        utf8("请把下面的本机码发给安装人员，收到激活码后粘贴到输入框。\n\n本机码：") + license.getMachineCode(),
        juce::MessageBoxIconType::QuestionIcon);
    activationDialog->addTextEditor("code", {}, utf8("唯一激活码"));
    activationDialog->addButton(utf8("确认激活"), 1, juce::KeyPress(juce::KeyPress::returnKey));
    activationDialog->addButton(utf8("取消"), 0, juce::KeyPress(juce::KeyPress::escapeKey));
    activationDialog->enterModalState(true, juce::ModalCallbackFunction::create([this](int result)
    {
        if (result == 1 && activationDialog != nullptr)
        {
            const auto status = license.activate(activationDialog->getTextEditorContents("code"));
            refreshLicenseUi();
            juce::AlertWindow::showMessageBoxAsync(status.activated ? juce::MessageBoxIconType::InfoIcon
                                                                    : juce::MessageBoxIconType::WarningIcon,
                                                   status.activated ? utf8("激活成功") : utf8("无法激活"), status.message);
        }
        activationDialog.reset();
    }), false);
}

void MainComponent::toggleRecording()
{
    if (recorder.isRecording())
    {
        recorder.stop();
        recordButton.setButtonText(utf8("● 开始录音"));
        recordButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff253b52));
        recordingStatus.setText(utf8("已保存：") + recorder.getLastFile().getFullPathName(), juce::dontSendNotification);
        return;
    }

    const auto status = audio.getStatus();
    auto* device = audio.getDeviceManager().getCurrentAudioDevice();
    const auto channels = device != nullptr ? device->getActiveOutputChannels().countNumberOfSetBits() : 0;
    const auto recordingName = pluginHost.hasPlugin()
        ? utf8(fengyin::SwamPluginClassifier::instrumentChineseName(pluginHost.getPluginName().toStdString()))
        : utf8("测试音源");
    if (! recorder.start(status.sampleRate, juce::jmax(1, channels), recordingName))
    {
        recordingStatus.setText(recorder.getLastError(), juce::dontSendNotification);
        return;
    }
    recordButton.setButtonText(utf8("■ 停止并保存"));
    recordButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xffa73545));
    recordingStatus.setText(utf8("正在录音  00:00"), juce::dontSendNotification);
}

void MainComponent::emitAudioSettingsState(bool success, const juce::String& message)
{
    if (webInterface == nullptr || ! webInterfaceReady) return;

    const auto status = audio.getStatus();
    auto result = std::make_unique<juce::DynamicObject>();
    result->setProperty("success", success);
    result->setProperty("message", message);
    result->setProperty("type", status.deviceType);
    result->setProperty("output", status.deviceName);
    result->setProperty("sampleRate", status.sampleRate);
    result->setProperty("bufferSize", status.bufferSize);
    result->setProperty("latency", status.estimatedBufferLatencyMs);
    result->setProperty("lowLatencyMode", status.deviceType.containsIgnoreCase("Low Latency Mode")
                                              || status.deviceType.containsIgnoreCase(utf8("低延迟"))
                                              || status.deviceType.containsIgnoreCase("ASIO"));

    juce::Array<juce::var> types;
    for (const auto& item : audio.getAvailableDeviceTypes()) types.add(item);
    result->setProperty("types", juce::var(types));

    juce::Array<juce::var> outputs;
    for (const auto& item : audio.getAvailableOutputDevices(status.deviceType)) outputs.add(item);
    result->setProperty("outputs", juce::var(outputs));

    juce::Array<juce::var> rates;
    for (const auto item : audio.getAvailableSampleRates()) rates.add(item);
    result->setProperty("sampleRates", juce::var(rates));

    juce::Array<juce::var> buffers;
    for (const auto item : audio.getAvailableBufferSizes()) buffers.add(item);
    result->setProperty("bufferSizes", juce::var(buffers));
    webInterface->emitEventIfBrowserIsVisible("audioSettingsState", juce::var(result.release()));
}

void MainComponent::applyAudioSettingsFromWeb(const juce::var& payload)
{
    const auto previous = audio.getStatus();
    if (recorder.isRecording()) toggleRecording();

    const auto requestedType = payload.getProperty("type", previous.deviceType).toString();
    const auto requestedOutput = payload.getProperty("output", previous.deviceName).toString();
    const auto requestedRate = static_cast<double>(payload.getProperty("sampleRate", previous.sampleRate));
    const auto requestedBuffer = static_cast<int>(payload.getProperty("bufferSize", previous.bufferSize));

    auto error = requestedType == previous.deviceType ? juce::String() : audio.selectDeviceType(requestedType);
    if (error.isEmpty())
    {
        const auto outputs = audio.getAvailableOutputDevices(requestedType);
        auto output = outputs.contains(requestedOutput) ? requestedOutput : audio.getStatus().deviceName;
        if (! outputs.contains(output) && ! outputs.isEmpty()) output = outputs[0];

        auto sampleRate = requestedRate > 0.0 ? requestedRate : 48000.0;
        const auto rates = audio.getAvailableSampleRates();
        if (! rates.isEmpty() && ! rates.contains(sampleRate))
        {
            sampleRate = rates[0];
            for (const auto candidate : rates)
                if (std::abs(candidate - requestedRate) < std::abs(sampleRate - requestedRate)) sampleRate = candidate;
        }

        auto bufferSize = requestedBuffer > 0 ? requestedBuffer : 128;
        const auto buffers = audio.getAvailableBufferSizes();
        if (! buffers.isEmpty() && ! buffers.contains(bufferSize))
        {
            bufferSize = buffers[0];
            for (const auto candidate : buffers)
                if (std::abs(candidate - requestedBuffer) < std::abs(bufferSize - requestedBuffer)) bufferSize = candidate;
        }
        error = output.isEmpty() ? utf8("没有可用的声音输出设备")
                                 : audio.applyOutputSetup(output, sampleRate, bufferSize);
    }

    if (error.isNotEmpty() && previous.ready && previous.deviceType.isNotEmpty())
    {
        (void) audio.selectDeviceType(previous.deviceType);
        (void) audio.applyOutputSetup(previous.deviceName, previous.sampleRate, previous.bufferSize);
    }

    emitAudioSettingsState(error.isEmpty(), error.isEmpty()
        ? utf8("设置已保存并立即生效")
        : utf8("无法应用，已恢复上一个可用设置：") + error);
}

void MainComponent::showDeviceSettings()
{
    const auto current = audio.getStatus();
    const auto types = audio.getAvailableDeviceTypes();
    deviceDialog = std::make_unique<juce::AlertWindow>(utf8("声音设备设置"),
        utf8("推荐：普通电脑优先使用低延迟模式、48000 Hz、128 缓冲区；专业声卡可选厂家 ASIO。"),
        juce::MessageBoxIconType::QuestionIcon);
    deviceDialog->addComboBox("type", types, utf8("声音驱动"));
    auto* typeBox = deviceDialog->getComboBoxComponent("type");
    typeBox->setText(current.deviceType, juce::dontSendNotification);
    deviceDialog->addComboBox("output", audio.getAvailableOutputDevices(current.deviceType), utf8("输出设备"));
    auto* outputBox = deviceDialog->getComboBoxComponent("output");
    outputBox->setText(current.deviceName, juce::dontSendNotification);
    deviceDialog->addComboBox("rate", { utf8("44100 Hz"), utf8("48000 Hz（推荐）"), utf8("96000 Hz") }, utf8("采样率"));
    auto* rateBox = deviceDialog->getComboBoxComponent("rate");
    rateBox->setSelectedId(current.sampleRate >= 88000.0 ? 3 : (current.sampleRate >= 46000.0 ? 2 : 1), juce::dontSendNotification);
    deviceDialog->addComboBox("buffer", { utf8("64（低延迟）"), utf8("128（推荐）"), utf8("256（更稳定）"), utf8("512（最稳定）") }, utf8("缓冲区"));
    auto* bufferBox = deviceDialog->getComboBoxComponent("buffer");
    const auto bufferId = current.bufferSize <= 64 ? 1 : current.bufferSize <= 128 ? 2 : current.bufferSize <= 256 ? 3 : 4;
    bufferBox->setSelectedId(bufferId, juce::dontSendNotification);
    typeBox->onChange = [this, typeBox, outputBox]
    {
        outputBox->clear(juce::dontSendNotification);
        outputBox->addItemList(audio.getAvailableOutputDevices(typeBox->getText()), 1);
        if (outputBox->getNumItems() > 0) outputBox->setSelectedId(1, juce::dontSendNotification);
    };
    deviceDialog->addButton(utf8("应用设置"), 1, juce::KeyPress(juce::KeyPress::returnKey));
    deviceDialog->addButton(utf8("取消"), 0, juce::KeyPress(juce::KeyPress::escapeKey));
    deviceDialog->enterModalState(true, juce::ModalCallbackFunction::create([this](int result)
    {
        if (result == 1 && deviceDialog != nullptr)
        {
            if (recorder.isRecording()) toggleRecording();
            const auto type = deviceDialog->getComboBoxComponent("type")->getText();
            const auto output = deviceDialog->getComboBoxComponent("output")->getText();
            const int rates[] { 44100, 48000, 96000 };
            const int buffers[] { 64, 128, 256, 512 };
            auto error = audio.selectDeviceType(type);
            if (error.isEmpty())
            {
                const auto rateId = juce::jlimit(1, 3, deviceDialog->getComboBoxComponent("rate")->getSelectedId());
                const auto selectedBufferId = juce::jlimit(1, 4, deviceDialog->getComboBoxComponent("buffer")->getSelectedId());
                error = audio.applyOutputSetup(output, rates[rateId - 1], buffers[selectedBufferId - 1]);
            }
            juce::AlertWindow::showMessageBoxAsync(error.isEmpty() ? juce::MessageBoxIconType::InfoIcon
                                                                    : juce::MessageBoxIconType::WarningIcon,
                error.isEmpty() ? utf8("设置已应用") : utf8("无法应用设置"),
                error.isEmpty() ? utf8("声音设备设置已保存，下次启动会自动使用。") : error);
        }
        deviceDialog.reset();
    }), false);
}

void MainComponent::showMidiSetup()
{
    midiDialogDevices = midi.getAvailableDevices();
    if (midiDialogDevices.empty())
    {
        juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon, utf8("没有发现电吹管"),
            utf8("请用 USB 线连接电吹管并打开电源，然后点击“连接向导”重试。"));
        return;
    }
    juce::StringArray names;
    for (const auto& device : midiDialogDevices) names.add(device.name);
    midiDialog = std::make_unique<juce::AlertWindow>(utf8("电吹管连接向导"),
        utf8("选择设备。常见电吹管会自动匹配；如果吹奏时气息没有变化，请使用“吹长音自动识别”。"),
        juce::MessageBoxIconType::QuestionIcon);
    midiDialog->addComboBox("midiDevice", names, utf8("电吹管设备"));
    auto* deviceBox = midiDialog->getComboBoxComponent("midiDevice");
    deviceBox->setSelectedId(1, juce::dontSendNotification);
    for (int i = 0; i < static_cast<int>(midiDialogDevices.size()); ++i)
        if (midiDialogDevices[static_cast<size_t>(i)].name == midi.getConnectedDeviceName())
            deviceBox->setSelectedId(i + 1, juce::dontSendNotification);
    midiDialog->addComboBox("breathCC", { utf8("CC2（大多数电吹管）"), utf8("CC11（表情）"), utf8("CC1（调制）") }, utf8("气息控制器"));
    auto* ccBox = midiDialog->getComboBoxComponent("breathCC");
    const auto currentCc = midi.getBreathController();
    ccBox->setSelectedId(currentCc == 11 ? 2 : currentCc == 1 ? 3 : 1, juce::dontSendNotification);
    midiDialog->addButton(utf8("连接并保存"), 1, juce::KeyPress(juce::KeyPress::returnKey));
    midiDialog->addButton(utf8("吹长音自动识别"), 2);
    midiDialog->addButton(utf8("取消"), 0, juce::KeyPress(juce::KeyPress::escapeKey));
    midiDialog->enterModalState(true, juce::ModalCallbackFunction::create([this](int result)
    {
        if ((result == 1 || result == 2) && midiDialog != nullptr)
        {
            const auto index = midiDialog->getComboBoxComponent("midiDevice")->getSelectedId() - 1;
            if (juce::isPositiveAndBelow(index, static_cast<int>(midiDialogDevices.size())))
            {
                const auto identifier = midiDialogDevices[static_cast<size_t>(index)].identifier;
                if (! midi.connect(identifier))
                    juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon, utf8("连接失败"),
                                                           utf8("请重新插拔设备后再试。"));
                else if (result == 2)
                    startBreathDetection(identifier);
                else
                {
                    const int controllers[] { 2, 11, 1 };
                    const auto selected = juce::jlimit(1, 3, midiDialog->getComboBoxComponent("breathCC")->getSelectedId());
                    midi.setBreathController(controllers[selected - 1]);
                }
            }
        }
        midiDialog.reset();
    }), false);
}

void MainComponent::startBreathDetection(const juce::String&)
{
    automaticBreathDetectionActive = false;
    automaticBreathDetectionEndsAtMs = 0.0;
    midi.beginBreathDetection();
    breathDetectionActive = true;
    breathDetectionEndsAtMs = juce::Time::getMillisecondCounterHiRes() + 6000.0;
}

void MainComponent::showExpressionSettings()
{
    const auto current = midi.getExpressionSettings();
    expressionDialog = std::make_unique<juce::AlertWindow>(utf8("吹奏手感调节"),
        utf8("推荐先使用“自然、均衡、标准”。气息抖动时调得更稳定，轻吹不响时调得更灵敏。"),
        juce::MessageBoxIconType::QuestionIcon);
    expressionDialog->addComboBox("response", { utf8("灵敏（轻吹更容易响）"), utf8("自然（推荐）"), utf8("稳重（强吹变化更明显）") }, utf8("气息响应"));
    expressionDialog->getComboBoxComponent("response")->setSelectedId(current.curve < 0.8f ? 1 : current.curve > 1.2f ? 3 : 2,
                                                                        juce::dontSendNotification);
    expressionDialog->addComboBox("smooth", { utf8("快速（变化最灵敏）"), utf8("均衡（推荐）"), utf8("稳定（减少气息抖动）") }, utf8("平滑程度"));
    expressionDialog->getComboBoxComponent("smooth")->setSelectedId(current.smoothing > 0.38f ? 1 : current.smoothing < 0.2f ? 3 : 2,
                                                                      juce::dontSendNotification);
    expressionDialog->addComboBox("threshold", { utf8("灵敏（适合轻吹）"), utf8("均衡（推荐）"), utf8("防误触（过滤微弱气流）") }, utf8("起音门槛"));
    expressionDialog->getComboBoxComponent("threshold")->setSelectedId(current.threshold < 0.018f ? 1 : current.threshold > 0.045f ? 3 : 2,
                                                                         juce::dontSendNotification);
    expressionDialog->addComboBox("pitch", { utf8("轻柔（弯音幅度较小）"), utf8("标准（推荐）"), utf8("宽广（弯音幅度更大）") }, utf8("弯音灵敏度"));
    expressionDialog->getComboBoxComponent("pitch")->setSelectedId(current.pitchSensitivity < 0.75f ? 1 : current.pitchSensitivity > 1.25f ? 3 : 2,
                                                                     juce::dontSendNotification);
    expressionDialog->addButton(utf8("应用"), 1, juce::KeyPress(juce::KeyPress::returnKey));
    expressionDialog->addButton(utf8("恢复推荐"), 2);
    expressionDialog->addButton(utf8("取消"), 0, juce::KeyPress(juce::KeyPress::escapeKey));
    expressionDialog->enterModalState(true, juce::ModalCallbackFunction::create([this](int result)
    {
        if ((result == 1 || result == 2) && expressionDialog != nullptr)
        {
            fengyin::ExpressionSettings next;
            if (result == 1)
            {
                const float curves[] { 0.65f, 0.9f, 1.45f };
                const float smoothing[] { 0.48f, 0.28f, 0.14f };
                const float thresholds[] { 0.008f, 0.02f, 0.06f };
                const float pitch[] { 0.5f, 1.0f, 1.5f };
                const auto selected = [this](const char* name)
                { return juce::jlimit(1, 3, expressionDialog->getComboBoxComponent(name)->getSelectedId()) - 1; };
                next = { thresholds[selected("threshold")], curves[selected("response")],
                         smoothing[selected("smooth")], pitch[selected("pitch")] };
            }
            midi.setExpressionSettings(next);
            juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::InfoIcon, utf8("吹奏手感已更新"),
                                                   utf8("设置已保存，您现在可以直接试吹。"));
        }
        expressionDialog.reset();
    }), false);
}

juce::String MainComponent::midiNoteName(int note)
{
    static constexpr const char* names[] { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    return juce::String(names[note % 12]) + juce::String(note / 12 - 1);
}
