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
    setupNav(chainNav, "◇  定制音色", Page::chain);
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
    // 设备能力选择已完成；真实稳定性验证必须等软音源载入后再运行，
    // 否则空载 CPU/xrun 会对用户报出虚假的“优化完成”。
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
            emitAudioSettingsState(true, audio.isAutomaticLatencyTuning()
                ? utf8("正在自动优化声音，无需操作…") : juce::String());
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
        .withEventListener("loadKongInstrument", [this](juce::var payload)
        {
            if (isActivated)
                loadKongInstrument(payload.getProperty("instrumentKey", {}).toString(),
                                   payload.getProperty("instrumentName", {}).toString());
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
                captureToneBeforePresetEdit();
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
        .withEventListener("cancelPresetEdit", [this](juce::var)
        {
            editingPresetId.clear();
            restoreToneBeforePresetEdit();
        })
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
            if (isActivated)
                midi.setTransposeSemitones(static_cast<int>(payload.getProperty("semitones", 0)));
        })
        .withEventListener("setKeyTranspose", [this](juce::var payload)
        {
            if (isActivated)
                midi.setTargetKey(static_cast<int>(payload.getProperty("targetKey", 0)));
        })
        .withEventListener("setGrowlSensitivity", [this](juce::var payload)
        {
            const auto value = payload.getProperty("sensitivity", "standard").toString();
            midi.setGrowlSensitivity(value == "easy" ? 0 : value == "hard" ? 2 : 1);
        })
        .withEventListener("copyMachineCode", [this](juce::var)
        {
            juce::SystemClipboard::copyTextToClipboard(machineCode);
        })
        .withEventListener("chooseVideo", [this](juce::var) { if (isActivated) chooseVideoForWebInterface(); })
        .withEventListener("setVideoPlaybackState", [this](juce::var payload)
        {
            if (static_cast<int>(payload.getProperty("generation", -1)) != webVideoGeneration) return;
            if (! isActivated)
            {
                accompaniment.pause();
                return;
            }
            videoPlaybackActive = static_cast<bool>(payload.getProperty("playing", false));
            webVideoPosition = static_cast<double>(payload.getProperty("position", webVideoPosition));
            audioOutputSyncTicks = 0;
            if (videoPlaybackActive)
            {
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
            if (static_cast<int>(payload.getProperty("generation", -1)) != webVideoGeneration) return;
            webVideoPosition = static_cast<double>(payload.getProperty("position", webVideoPosition));
            if (webVideoAudioReady && std::abs(accompaniment.getPosition() - webVideoPosition) > 0.35)
                accompaniment.setPosition(webVideoPosition);
        })
        .withEventListener("seekVideo", [this](juce::var payload)
        {
            if (static_cast<int>(payload.getProperty("generation", -1)) != webVideoGeneration) return;
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
            if (isActivated)
                masterOutput.setReverbMix(static_cast<float>(static_cast<double>(payload.getProperty("value", 0.28))));
        })
        .withEventListener("setToneStyle", [this](juce::var payload)
        {
            if (! isActivated || ! pluginHost.hasPlugin()) return;
            const auto instrumentKey = currentInstrumentKey.isNotEmpty() ? currentInstrumentKey
                : utf8(fengyin::SwamPluginClassifier::instrumentKey(pluginHost.getPluginName().toStdString()));
            const auto style = fengyin::ToneStyleCatalog::find(instrumentKey,
                payload.getProperty("id", "natural").toString());
            currentToneStyleId = style.id;
            currentPresetDisplayName.clear();
            currentPresetId.clear();
            currentPresetIsCustom = false;
            currentBaseToneSettings = style.settings;
            masterOutput.setToneStyle(style.settings);
            currentSwamToneParameterCount = currentPluginBrand == "swam"
                ? pluginHost.applySwamToneProfile(style.swam) : 0;
        })
        .withEventListener("setInstrumentModel", [this](juce::var payload)
        {
            if (! isActivated || currentPluginBrand != "swam" || ! pluginHost.hasPlugin()) return;
            const auto index = static_cast<int>(payload.getProperty("index", -1));
            if (! pluginHost.selectInstrumentModel(index)) return;
            applyCurrentSwamToneStyle();
            pluginStatus.setText(utf8("已切换乐器型号：") + pluginHost.getCurrentInstrumentModelName(),
                                 juce::dontSendNotification);
        })
        .withEventListener("previewCustomTone", [this](juce::var payload)
        {
            if (isActivated && pluginHost.hasPlugin())
                masterOutput.setToneStyle(customToneSettingsFromPayload(payload));
        })
        .withEventListener("cancelCustomTone", [this](juce::var)
        {
            if (pluginHost.hasPlugin()) masterOutput.setToneStyle(currentBaseToneSettings);
        })
        .withEventListener("saveCustomPreset", [this](juce::var payload)
        {
            if (! isActivated || ! pluginHost.hasPlugin()) return;
            const auto name = payload.getProperty("name", {}).toString().trim();
            const auto baseStyleId = payload.getProperty("baseStyleId", currentToneStyleId).toString();
            const auto settings = payload.getProperty("settings", juce::var());
            masterOutput.setToneStyle(customToneSettingsFromPayload(settings));
            commitCustomPreset(name, baseStyleId);
        })
        .withEventListener("setSmartOptimisation", [this](juce::var payload)
        {
            const auto enabled = static_cast<bool>(payload.getProperty("enabled", true));
            masterOutput.setSmartOptimisationEnabled(enabled);
            // Tone assistance never reconfigures the output device.
        })
        .withEventListener("requestAudioSettings", [this](juce::var) { emitAudioSettingsState(); })
        .withEventListener("applyAudioSettings", [this](juce::var payload) { applyAudioSettingsFromWeb(payload); })
        .withEventListener("optimiseAudioSettings", [this](juce::var)
        {
            if (videoPlaybackActive || recorder.isRecording() || midi.getSnapshot().breath > 0.01f
                || midi.getSnapshot().lastNote >= 0)
                emitAudioSettingsState(false, utf8("请先暂停伴奏、录音和吹奏，再优化声音"));
            else if (! pluginHost.hasPlugin())
                emitAudioSettingsState(false, utf8("请先选择一个音色方案，再进行真实音源优化"));
            else if (audio.beginAutomaticLatencyTuning(true))
            {
                pluginHost.setLatencyProbeActive(true);
                emitAudioSettingsState(true, utf8("正在自动优化声音，无需操作…"));
            }
            else
                emitAudioSettingsState(false, utf8("当前无法开始自动优化，请检查声音输出设备"));
        })
        .withEventListener("beginTechniqueLearn", [this](juce::var payload)
        {
            const auto requestedId = payload.getProperty("techniqueId", {}).toString().toStdString();
            const auto requestedTechnique = requestedId.empty()
                ? static_cast<fengyin::PerformanceTechnique>(juce::jlimit(0, static_cast<int>(fengyin::PerformanceTechnique::count) - 1,
                                                                         static_cast<int>(payload.getProperty("technique", 0))))
                : fengyin::techniqueFromId(requestedId);
            if (requestedTechnique == fengyin::PerformanceTechnique::count) return;
            const auto technique = static_cast<int>(requestedTechnique);
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
            activeTechniqueLearn = technique;
            pendingTechniqueToggle = static_cast<bool>(payload.getProperty("toggle", false));
            techniqueLearnEndsAtMs = juce::Time::getMillisecondCounterHiRes() + 10000.0;
            midi.beginTechniqueLearn(static_cast<fengyin::PerformanceTechnique>(technique));
        })
        .withEventListener("setTechniqueConfiguration", [this](juce::var payload)
        {
            const auto target = fengyin::techniqueFromId(payload.getProperty("techniqueId", {}).toString().toStdString());
            if (target == fengyin::PerformanceTechnique::count) return;
            const auto modeText = payload.getProperty("mode", "auto").toString();
            auto mode = fengyin::TechniqueControlMode::automatic;
            if (modeText == "breath") mode = fengyin::TechniqueControlMode::breath;
            else if (modeText == "hardware") mode = fengyin::TechniqueControlMode::hardware;
            else if (modeText == "hybrid") mode = fengyin::TechniqueControlMode::hybrid;
            else if (modeText == "off") mode = fengyin::TechniqueControlMode::off;
            auto mappings = midi.getTechniqueMappings();
            auto found = false;
            for (auto& mapping : mappings)
                if (mapping.technique == target)
                {
                    mapping.mode = mode;
                    mapping.strength = juce::jlimit(0.0f, 1.0f,
                        static_cast<float>(static_cast<double>(payload.getProperty("strength", mapping.strength))));
                    found = true;
                    break;
                }
            if (! found)
            {
                fengyin::TechniqueMapping mapping;
                mapping.technique = target;
                mapping.mode = mode;
                mapping.strength = juce::jlimit(0.0f, 1.0f,
                    static_cast<float>(static_cast<double>(payload.getProperty("strength", 0.5))));
                mappings.add(mapping);
            }
            midi.setTechniqueMappings(mappings);
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
            masterOutput.setWarmth(static_cast<float>(static_cast<double>(payload.getProperty("warmth", masterOutput.getWarmth()))));
            masterOutput.setReverbMix(static_cast<float>(static_cast<double>(payload.getProperty("reverb", 0.28))));
            masterOutput.setLimiterCeiling(static_cast<float>(static_cast<double>(payload.getProperty("limiter", 0.95))));
        })
        .withEventListener("showMidiSetup", [this](juce::var) { showMidiSetup(); })
        .withEventListener("showExpressionSettings", [this](juce::var) { showExpressionSettings(); })
        .withEventListener("showAudioSettings", [this](juce::var) { showDeviceSettings(); })
        .withEventListener("toggleRecording", [this](juce::var) { if (isActivated || recorder.isRecording()) toggleRecording(); })
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
            emitLicenseState(status);
        })
        .withEventListener("startTrial", [this](juce::var)
        {
            const auto status = license.startTrial();
            refreshLicenseUi();
            emitLicenseState(status);
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
    const auto generation = ++webVideoGeneration;
    videoPlaybackActive = false;
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
        result->setProperty("generation", generation);
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
        [safeThis = juce::Component::SafePointer<MainComponent>(this), file, generation](bool success, const juce::File& audioFile, const juce::String& error)
        {
            if (safeThis == nullptr || safeThis->webVideoGeneration != generation || safeThis->webVideoFile != file) return;
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
    result->setProperty("generation", webVideoGeneration);
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
    const juce::String names[] { utf8("开始演奏"), utf8("音色方案"), utf8("定制音色"),
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
    for (auto* component : std::array<juce::Component*, 5> { &presetSelector, &savePresetButton, &loadPresetButton,
                             &defaultPresetButton, &deletePresetButton })
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
        const juce::String descriptions[] { {}, utf8("命名、保存并快速恢复完整的演奏音色。"),
            utf8("按从左到右的顺序管理音源、效果器和最终输出。"),
            utf8("连接电吹管并调整气息、起音与弯音手感。"),
            utf8("自动跟随 Windows 默认耳机或音响，并选择共享低延迟方案。"),
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
    if (++licensePollTicks >= 300)
    {
        licensePollTicks = 0;
        refreshLicenseUi();
    }
    if (++midiConnectionPollCounter >= 30)
    {
        midiConnectionPollCounter = 0;
        midi.pollConnection();
    }
    snapshot = midi.getSnapshot();
    if (snapshot.breath > 0.01f || snapshot.lastNote >= 0)
        lastPerformanceActivityMs = juce::Time::getMillisecondCounterHiRes();
    if (audio.isAutomaticLatencyTuning()
        && (videoPlaybackActive || recorder.isRecording() || snapshot.breath > 0.01f || snapshot.lastNote >= 0))
    {
        pluginHost.setLatencyProbeActive(false);
        audio.cancelAutomaticLatencyTuning();
        emitAudioSettingsState(false, utf8("已停止声音优化，恢复演奏"));
    }
    if (++audioOutputSyncTicks >= 60)
    {
        audioOutputSyncTicks = 0;
        const auto outputChanged = audio.systemDefaultOutputChanged();
        const auto safeToReconfigure = ! videoPlaybackActive && ! recorder.isRecording()
            && juce::Time::getMillisecondCounterHiRes() - lastPerformanceActivityMs > 3000.0;
        if (outputChanged)
            followSystemAudioOutputIfNeeded();
        if (! outputChanged && safeToReconfigure && ! pluginLoading && pluginHost.hasPlugin()
            && audio.needsAutomaticLatencyTuning()
            && audio.beginAutomaticLatencyTuning())
        {
            pluginHost.setLatencyProbeActive(true);
            emitAudioSettingsState(true, utf8("正在验证声音设置…"));
        }
    }
    if (audio.isAutomaticLatencyTuning()) emitAudioSettingsState();
    audio.updateProbeEvidence(pluginHost.getCallbackCount(), pluginHost.getCallbackOverruns(), pluginHost.getSignalBlocks());
    if (const auto tuningResult = audio.pollAutomaticLatencyTuning(); tuningResult.has_value())
    {
        pluginHost.setLatencyProbeActive(false);
        if (webInterface != nullptr)
            webInterface->emitEventIfBrowserIsVisible("audioOptimisationResult", *tuningResult);
        emitAudioSettingsState(! tuningResult->contains(utf8("未")), *tuningResult);
    }
    if (snapshot.deviceConnected && ! wasMidiConnected)
    {
        // 已知型号先使用档案中的推荐值，同时静默观察真实连续控制器；
        // 可兼容用户在吹管 App 中改过 MIDI 输出的情况。
        midi.beginBreathDetection();
        automaticBreathDetectionActive = true;
        automaticBreathDetectionEndsAtMs = 0.0;
        configureTechniqueDefaults();
    }
    else if (! snapshot.deviceConnected && wasMidiConnected)
    {
        automaticBreathDetectionActive = false;
        automaticBreathDetectionEndsAtMs = 0.0;
    }
    wasMidiConnected = snapshot.deviceConnected;
    if (automaticBreathDetectionActive && automaticBreathDetectionEndsAtMs <= 0.0)
        automaticBreathDetectionEndsAtMs = juce::Time::getMillisecondCounterHiRes() + 1800.0;
    if (automaticBreathDetectionActive && automaticBreathDetectionEndsAtMs > 0.0
        && juce::Time::getMillisecondCounterHiRes() >= automaticBreathDetectionEndsAtMs)
    {
        (void) midi.finishBreathDetection();
        automaticBreathDetectionActive = false;
        automaticBreathDetectionEndsAtMs = 0.0;
    }
    // Technique automation is consumed by the audio graph, not the UI timer.
    if (activeTechniqueLearn >= 0)
    {
        const auto learned = midi.consumeTechniqueLearnResult();
        if (learned.ready)
        {
            auto mappings = midi.getTechniqueMappings();
            auto previousMode = fengyin::TechniqueControlMode::hardware;
            auto previousStrength = 0.5f;
            for (int index = mappings.size(); --index >= 0;)
                if (mappings.getReference(index).technique == learned.mapping.technique)
                {
                    previousMode = mappings.getReference(index).mode;
                    previousStrength = mappings.getReference(index).strength;
                    mappings.remove(index);
                }
            auto mapping = learned.mapping;
            mapping.toggle = pendingTechniqueToggle;
            mapping.mode = previousMode == fengyin::TechniqueControlMode::hybrid
                ? previousMode : fengyin::TechniqueControlMode::hardware;
            mapping.strength = previousStrength;
            mappings.add(mapping);
            midi.setTechniqueMappings(mappings);
            activeTechniqueLearn = -1;
            techniqueLearnEndsAtMs = 0.0;
            if (webInterface != nullptr)
            {
                auto result = std::make_unique<juce::DynamicObject>();
                result->setProperty("success", true);
                result->setProperty("message", utf8("映射成功，已对这支电吹管全局保存"));
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
        // 运行中只告警，绝不在演奏背后改缓冲或重启音频设备。
        if (masterOutput.isSmartOptimisationEnabled()
            && audio.hasSustainedRuntimeInstability() && webInterface != nullptr)
        {
            const auto message = utf8("检测到持续丢音，演奏未被中断；可在声音设置中重新优化");
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
        state->setProperty("callbackOverruns", static_cast<juce::int64>(pluginHost.getCallbackOverruns()));
        state->setProperty("overloadSamples", static_cast<juce::int64>(masterOutput.getOverloadSamples()));
        state->setProperty("droppedMidiEvents", static_cast<juce::int64>(pluginHost.getDroppedMidiEventCount()));
        state->setProperty("activated", currentLicenseStatus.activated);
        state->setProperty("featuresUnlocked", currentLicenseStatus.canUseFeatures());
        state->setProperty("trialActive", currentLicenseStatus.trialActive);
        state->setProperty("trialExpired", currentLicenseStatus.trialExpired);
        state->setProperty("trialRemainingSeconds", currentLicenseStatus.trialRemainingSeconds);
        state->setProperty("licenseMessage", currentLicenseStatus.message);
        state->setProperty("machineCode", machineCode);
        state->setProperty("recording", recorder.isRecording());
        state->setProperty("transposeSemitones", midi.getTransposeSemitones());
        state->setProperty("targetKey", midi.getTargetKey());
        state->setProperty("growlSensitivity", midi.getGrowlSensitivity());
        state->setProperty("automaticBreathDetection", automaticBreathDetectionActive);
        state->setProperty("reverbMix", masterOutput.getReverbMix());
        state->setProperty("eqTone", masterOutput.getEqTone());
        state->setProperty("toneWarmth", masterOutput.getWarmth());
        state->setProperty("toneStyleId", currentToneStyleId);
        const auto toneSettings = masterOutput.getToneStyle();
        auto toneObject = std::make_unique<juce::DynamicObject>();
        toneObject->setProperty("brightness", (toneSettings.tone + 1.0f) * 50.0f);
        toneObject->setProperty("bass", (toneSettings.bass + 1.0f) * 50.0f);
        toneObject->setProperty("saturation", toneSettings.saturation / 0.35f * 100.0f);
        toneObject->setProperty("warmth", toneSettings.warmth * 100.0f);
        toneObject->setProperty("compression", juce::jlimit(0.0f, 100.0f, (0.85f - toneSettings.compressionThreshold) / 0.55f * 100.0f));
        toneObject->setProperty("ratio", (toneSettings.compressionRatio - 1.0f) / 3.0f * 100.0f);
        toneObject->setProperty("damping", toneSettings.reverbDamping * 100.0f);
        toneObject->setProperty("width", toneSettings.reverbWidth * 100.0f);
        toneObject->setProperty("harsh", toneSettings.harshControl / 0.35f * 100.0f);
        toneObject->setProperty("reverb", toneSettings.reverbMix / 0.6f * 100.0f);
        toneObject->setProperty("room", toneSettings.reverbRoomSize * 100.0f);
        toneObject->setProperty("output", (toneSettings.outputGain - 0.5f) / 0.75f * 100.0f);
        state->setProperty("toneSettings", juce::var(toneObject.release()));
        state->setProperty("limiterCeiling", masterOutput.getLimiterCeiling());
        state->setProperty("smartOptimisation", masterOutput.isSmartOptimisationEnabled());
        const auto currentPluginName = pluginHost.hasPlugin() ? pluginHost.getPluginName() : juce::String();
        state->setProperty("pluginName", pluginHost.hasPlugin() ? currentPluginName : utf8("安全测试音源"));
        state->setProperty("pluginLoaded", pluginHost.hasPlugin());
        state->setProperty("pluginLoading", pluginLoading);
        state->setProperty("pluginBrand", currentPluginBrand);
        state->setProperty("instrumentChineseName", currentInstrumentChineseName);
        state->setProperty("instrumentKey", currentInstrumentKey);
        juce::Array<juce::var> instrumentModels;
        const auto swamModelNames = currentPluginBrand == "swam" ? pluginHost.getInstrumentModelNames()
                                                                  : juce::StringArray();
        for (int index = 0; index < swamModelNames.size(); ++index)
        {
            auto model = std::make_unique<juce::DynamicObject>();
            model->setProperty("id", juce::String(index));
            model->setProperty("name", swamModelNames[index]);
            instrumentModels.add(juce::var(model.release()));
        }
        state->setProperty("instrumentModels", juce::var(instrumentModels));
        state->setProperty("instrumentModelId", pluginHost.getCurrentInstrumentModelIndex());
        state->setProperty("instrumentModelName", pluginHost.getCurrentInstrumentModelName());
        state->setProperty("swamToneParameterCount", currentSwamToneParameterCount);
        state->setProperty("scanning", scanProgress.scanning);
        state->setProperty("scanProgress", scanProgress.fraction);
        state->setProperty("pluginStatus", pluginStatus.getText());
        state->setProperty("effectName", pluginHost.hasEffect() ? pluginHost.getEffectName() : juce::String());
        state->setProperty("effectLoaded", pluginHost.hasEffect());
        state->setProperty("effectLoading", effectLoading);
        state->setProperty("presetEditing", editingPresetId.isNotEmpty());
        state->setProperty("activePresetName", currentPresetDisplayName);
        state->setProperty("activePresetCustom", currentPresetIsCustom);
        state->setProperty("activePresetId", currentPresetId);
        state->setProperty("appVersion", JUCE_APPLICATION_VERSION_STRING);
        state->setProperty("activeToneVariantId", currentPresetIsCustom && currentPresetId.isNotEmpty()
            ? "custom:" + currentPresetId
            : currentInstrumentKey.isNotEmpty() ? "builtin:" + currentInstrumentKey + ":" + currentToneStyleId
                                                : juce::String());
        state->setProperty("techniqueLearning", activeTechniqueLearn);
        const auto currentFamily = currentTechniqueFamily();
        state->setProperty("instrumentFamily", utf8(fengyin::SwamPluginClassifier::familyChineseName(currentFamily)));
        juce::Array<juce::var> techniqueMappings;
        struct RoleDescription { fengyin::PerformanceTechnique role; const char* id; const char* name; };
        for (const auto& role : {
                RoleDescription { fengyin::PerformanceTechnique::vibrato, "vibrato", "颤动控制" },
                RoleDescription { fengyin::PerformanceTechnique::growl, "growl", "质感控制" },
                RoleDescription { fengyin::PerformanceTechnique::portamento, "portamento", "滑音控制" },
                RoleDescription { fengyin::PerformanceTechnique::mute, "mute", "特殊技巧" } })
        {
            auto item = std::make_unique<juce::DynamicObject>();
            item->setProperty("technique", static_cast<int>(role.role));
            item->setProperty("id", role.id);
            item->setProperty("name", utf8(role.name));
            item->setProperty("relevant", true);
            item->setProperty("pluginSupported", true);
            item->setProperty("hardwareAvailable", true);
            item->setProperty("supported", true);
            item->setProperty("recommendedSource", utf8("映射一次，切换乐器继续使用"));
            item->setProperty("recommendationReason", utf8("全局控制角色"));
            item->setProperty("defaultMode", static_cast<int>(fengyin::TechniqueControlMode::breath));
            item->setProperty("featured", true);
            item->setProperty("sourceType", 0);
            item->setProperty("sourceNumber", -1);
            item->setProperty("toggle", false);
            item->setProperty("mode", static_cast<int>(fengyin::TechniqueControlMode::breath));
            item->setProperty("strength", 0.5f);
            for (const auto& mapping : midi.getTechniqueMappings())
                if (mapping.technique == role.role)
                {
                    item->setProperty("sourceType", static_cast<int>(mapping.sourceType));
                    item->setProperty("sourceNumber", mapping.sourceNumber);
                    item->setProperty("toggle", mapping.toggle);
                    item->setProperty("mode", static_cast<int>(mapping.mode));
                    item->setProperty("strength", mapping.strength);
                    break;
                }
            techniqueMappings.add(juce::var(item.release()));
        }
        state->setProperty("techniqueMappings", juce::var(techniqueMappings));
        juce::Array<juce::var> instruments;
        for (const auto& plugin : cachedInstrumentPlugins)
        {
            const auto brand = fengyin::SupportedInstrumentClassifier::classify(plugin.name, plugin.manufacturerName);
            const auto isSwam = brand == fengyin::SupportedInstrumentClassifier::Brand::swam;
            const auto isKong = brand == fengyin::SupportedInstrumentClassifier::Brand::kong;
            auto item = std::make_unique<juce::DynamicObject>();
            item->setProperty("name", plugin.name);
            item->setProperty("isSwam", isSwam);
            item->setProperty("brand", isKong ? "kong" : isSwam ? "swam" : "unsupported");
            item->setProperty("supported", isSwam || isKong);
            item->setProperty("chineseName", isSwam
                ? utf8(fengyin::SwamPluginClassifier::instrumentChineseName(plugin.name.toStdString()))
                : isKong ? utf8("空音 Qin Engine") : plugin.name);
            item->setProperty("instrumentKey", isSwam
                ? utf8(fengyin::SwamPluginClassifier::instrumentKey(plugin.name.toStdString()))
                : isKong ? "kong-engine" : juce::String());
            item->setProperty("label", isSwam
                ? utf8(fengyin::SwamPluginClassifier::instrumentChineseName(plugin.name.toStdString())) + utf8(" · ") + plugin.name
                : isKong ? utf8("中国民乐 · ") + plugin.name : plugin.name + utf8("（暂未支持）"));
            instruments.add(juce::var(item.release()));
        }
        juce::Array<juce::var> effects;
        for (const auto& effect : cachedEffectPlugins) effects.add(effect.name);
        state->setProperty("instruments", juce::var(instruments));
        juce::Array<juce::var> kongInstruments;
        if (pluginHost.hasPlugin() && currentPluginBrand == "kong")
        {
            auto programs = pluginHost.getProgramNames();
            const auto currentProgram = pluginHost.getCurrentProgramName();
            if (currentProgram.isNotEmpty()) programs.addIfNotAlreadyThere(currentProgram);
            juce::StringArray addedKeys;
            for (const auto& program : programs)
                if (const auto* definition = fengyin::KongInstrumentCatalog::matchProgram(program))
                    if (! addedKeys.contains(definition->key))
                    {
                        auto item = std::make_unique<juce::DynamicObject>();
                        item->setProperty("key", definition->key);
                        item->setProperty("name", utf8(definition->chineseName));
                        item->setProperty("program", program);
                        kongInstruments.add(juce::var(item.release()));
                        addedKeys.add(definition->key);
                    }
        }
        state->setProperty("kongInstruments", juce::var(kongInstruments));
        state->setProperty("effects", juce::var(effects));
        juce::Array<juce::var> presets;
        for (const auto& preset : cachedPresets)
        {
            auto item = std::make_unique<juce::DynamicObject>();
            item->setProperty("id", preset.id);
            item->setProperty("name", preset.name);
            item->setProperty("brand", preset.pluginBrand);
            item->setProperty("instrumentKey", preset.instrumentKey);
            item->setProperty("instrumentChineseName", preset.instrumentChineseName);
            item->setProperty("customTone", preset.customTone);
            item->setProperty("baseToneStyleId", preset.baseToneStyleId);
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

void MainComponent::followSystemAudioOutputIfNeeded()
{
    if (! audio.systemDefaultOutputChanged()) return;

    // 切换 Windows 默认输出前先收音，避免正在发声的音符被遗留在旧设备。
    pluginHost.resetPerformance();
    testSynth.resetPerformance();
    if (recorder.isRecording()) toggleRecording();

    const auto shouldResumeAccompaniment = videoPlaybackActive && webVideoAudioReady;
    const auto resumePosition = webVideoAudioReady ? accompaniment.getPosition() : webVideoPosition;
    accompaniment.pause();
    const auto changed = audio.followSystemDefaultOutput();

    if (shouldResumeAccompaniment)
    {
        accompaniment.setPosition(resumePosition);
        accompaniment.play();
    }
    if (! changed) return;

    const auto status = audio.getStatus();
    const auto message = utf8("已自动切换声音输出：") + status.deviceName;
    if (webInterface != nullptr)
        webInterface->emitEventIfBrowserIsVisible("audioOptimisationResult", message);
    emitAudioSettingsState(true, message);
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
    bool kongHeadingAdded = false, otherHeadingAdded = false;
    for (const auto& plugin : plugins)
    {
        bool isSwam = false;
        for (const auto& swam : swamPlugins)
            if (swam.createIdentifierString() == plugin.createIdentifierString())
                isSwam = true;
        if (! isSwam && plugin.isInstrument)
        {
            const auto isKong = fengyin::SupportedInstrumentClassifier::classify(plugin.name, plugin.manufacturerName)
                              == fengyin::SupportedInstrumentClassifier::Brand::kong;
            if (isKong && ! kongHeadingAdded)
            {
                pluginSelector.addSectionHeading(utf8("中国民乐 · 空音"));
                kongHeadingAdded = true;
            }
            else if (! isKong && ! otherHeadingAdded)
            {
                pluginSelector.addSectionHeading(utf8("其他 VST3 音源"));
                otherHeadingAdded = true;
            }
            cachedInstrumentPlugins.add(plugin);
            pluginSelector.addItem(isKong ? utf8("空音 Qin Engine · ") + plugin.name
                                          : plugin.name + utf8("（当前版本暂未支持）"), itemId++);
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
    if (! fengyin::SupportedInstrumentClassifier::isSupported(chosen.name, chosen.manufacturerName))
    {
        const auto message = utf8("当前版本尚未支持 Kontakt、三体等其他音源，请关注后续版本升级。");
        pluginStatus.setText(message, juce::dontSendNotification);
        if (webInterface != nullptr)
        {
            auto result = std::make_unique<juce::DynamicObject>();
            result->setProperty("success", false);
            result->setProperty("message", message);
            webInterface->emitEventIfBrowserIsVisible("pluginLoadResult", juce::var(result.release()));
        }
        return;
    }

    const auto brand = fengyin::SupportedInstrumentClassifier::classify(chosen.name, chosen.manufacturerName);
    currentPresetDisplayName.clear();
    currentPresetId.clear();
    currentPresetIsCustom = false;
    currentPluginBrand = brand == fengyin::SupportedInstrumentClassifier::Brand::kong ? "kong" : "swam";
    if (currentPluginBrand == "swam")
    {
        currentInstrumentKey = utf8(fengyin::SwamPluginClassifier::instrumentKey(chosen.name.toStdString()));
        currentInstrumentChineseName = utf8(fengyin::SwamPluginClassifier::instrumentChineseName(chosen.name.toStdString()));
    }
    else
    {
        currentInstrumentKey = "kong-engine";
        currentInstrumentChineseName = utf8("空音 Qin Engine");
    }

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

void MainComponent::loadKongInstrument(const juce::String& instrumentKey, const juce::String& instrumentName)
{
    const auto* definition = fengyin::KongInstrumentCatalog::find(instrumentKey);
    if (definition == nullptr) return;
    const auto aliases = juce::StringArray::fromTokens(definition->programAliases, "|", "");
    if (pluginHost.hasPlugin() && currentPluginBrand == "kong")
    {
        currentInstrumentKey = instrumentKey;
        currentInstrumentChineseName = instrumentName.isNotEmpty() ? instrumentName : utf8(definition->chineseName);
        currentPresetDisplayName.clear();
        currentPresetId.clear();
        currentPresetIsCustom = false;
        const auto selected = pluginHost.selectProgramByAliases(aliases);
        activatePluginOutput(pluginHost.getPluginName());
        pluginStatus.setText(selected ? utf8("已载入：") + currentInstrumentChineseName
                                      : utf8("请在空音界面选择“") + currentInstrumentChineseName + utf8("”"),
                             juce::dontSendNotification);
        if (! selected) pluginHost.showPluginEditor(false);
        if (webInterface != nullptr)
        {
            auto result = std::make_unique<juce::DynamicObject>();
            result->setProperty("success", true);
            result->setProperty("message", pluginStatus.getText());
            webInterface->emitEventIfBrowserIsVisible("pluginLoadResult", juce::var(result.release()));
        }
        return;
    }
    juce::PluginDescription chosen;
    bool found = false;
    for (const auto& candidate : cachedInstrumentPlugins)
        if (fengyin::SupportedInstrumentClassifier::classify(candidate.name, candidate.manufacturerName)
            == fengyin::SupportedInstrumentClassifier::Brand::kong)
        {
            chosen = candidate;
            found = true;
            break;
        }
    if (! found)
    {
        const auto message = utf8("未找到空音 Qin Engine V3，请先安装后重新扫描音源。");
        pluginStatus.setText(message, juce::dontSendNotification);
        if (webInterface != nullptr)
        {
            auto result = std::make_unique<juce::DynamicObject>();
            result->setProperty("success", false);
            result->setProperty("message", message);
            webInterface->emitEventIfBrowserIsVisible("pluginLoadResult", juce::var(result.release()));
        }
        return;
    }
    currentPluginBrand = "kong";
    currentPresetDisplayName.clear();
    currentPresetId.clear();
    currentPresetIsCustom = false;
    currentInstrumentKey = instrumentKey;
    currentInstrumentChineseName = instrumentName.isNotEmpty() ? instrumentName : utf8(definition->chineseName);
    const auto status = audio.getStatus();
    pluginLoading = true;
    pluginStatus.setText(utf8("正在载入空音·") + currentInstrumentChineseName, juce::dontSendNotification);
    pluginHost.loadAsync(chosen, status.sampleRate > 0.0 ? status.sampleRate : 48000.0,
        status.bufferSize > 0 ? status.bufferSize : 128,
        [this, aliases](bool success, const juce::String& message)
        {
            pluginLoading = false;
            if (! success)
            {
                useTestSynth();
                pluginStatus.setText(utf8("空音载入失败：") + message, juce::dontSendNotification);
            }
            else
            {
                activatePluginOutput(message);
                const auto selected = pluginHost.selectProgramByAliases(aliases);
                pluginStatus.setText(selected ? utf8("已载入：") + currentInstrumentChineseName
                                              : utf8("已打开空音，请在音源界面选择“") + currentInstrumentChineseName + utf8("”"),
                                     juce::dontSendNotification);
                if (! selected) pluginHost.showPluginEditor(false);
            }
            if (webInterface != nullptr)
            {
                auto result = std::make_unique<juce::DynamicObject>();
                result->setProperty("success", success);
                result->setProperty("message", pluginStatus.getText());
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
    currentPluginBrand.clear();
    currentInstrumentKey.clear();
    currentInstrumentChineseName.clear();
}

void MainComponent::refreshPresetChoices()
{
    cachedPresets = presetStore.loadAll();
    std::sort(cachedPresets.begin(), cachedPresets.end(), [](const auto& a, const auto& b)
    { return a.name.compareIgnoreCase(b.name) < 0; });
    presetSelector.clear(juce::dontSendNotification);
    for (int i = 0; i < cachedPresets.size(); ++i)
    {
        const auto& preset = cachedPresets.getReference(i);
        const auto isDefault = preset.id == presetStore.getDefaultId();
        presetSelector.addItem(preset.name + (isDefault ? utf8(" · 默认") : juce::String()), i + 1);
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
    preset.toneParameters = pluginHost.captureToneParameters();
    preset.instrumentModelIndex = pluginHost.getCurrentInstrumentModelIndex();
    preset.eqTone = masterOutput.getEqTone();
    preset.warmth = masterOutput.getWarmth();
    preset.reverbMix = masterOutput.getReverbMix();
    preset.toneStyleId = currentToneStyleId;
    preset.baseToneStyleId = currentToneStyleId;
    preset.pluginBrand = currentPluginBrand;
    preset.instrumentKey = currentInstrumentKey;
    preset.instrumentChineseName = currentInstrumentChineseName;
    preset.pluginProgramName = pluginHost.getCurrentProgramName();
    preset.customTone = true;

    if (presetStore.save(preset))
    {
        const auto savedId = preset.id;
        editingPresetId.clear();
        editingReturnValid = false;
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

fengyin::ToneStyleSettings MainComponent::customToneSettingsFromPayload(const juce::var& payload) const
{
    auto result = currentBaseToneSettings;
    const auto normal = [&payload](const char* name, double fallback)
    {
        return juce::jlimit(0.0, 1.0, static_cast<double>(payload.getProperty(name, fallback * 100.0)) / 100.0);
    };
    const auto bass = normal("bass", 0.5);
    result.tone = static_cast<float>(normal("brightness", (result.tone + 1.0f) * 0.5f) * 2.0 - 1.0);
    result.bass = static_cast<float>(bass * 2.0 - 1.0);
    result.warmth = static_cast<float>(normal("warmth", result.warmth));
    const auto compression = normal("compression", 0.35);
    result.compressionThreshold = static_cast<float>(0.85 - compression * 0.55);
    result.compressionRatio = static_cast<float>(1.0 + normal("ratio", (result.compressionRatio - 1.0) / 3.0) * 3.0);
    result.saturation = static_cast<float>(normal("saturation", result.saturation / 0.35f) * 0.35);
    result.harshControl = static_cast<float>(normal("harsh", result.harshControl / 0.35f) * 0.35);
    result.reverbMix = static_cast<float>(normal("reverb", result.reverbMix / 0.6f) * 0.6);
    result.reverbRoomSize = static_cast<float>(normal("room", result.reverbRoomSize));
    result.reverbDamping = static_cast<float>(normal("damping", result.reverbDamping));
    result.reverbWidth = static_cast<float>(normal("width", result.reverbWidth));
    result.outputGain = static_cast<float>(0.5 + normal("output", 0.8) * 0.75);
    return result;
}

void MainComponent::commitCustomPreset(const juce::String& name, const juce::String& baseStyleId)
{
    if (name.isEmpty() || ! pluginHost.hasPlugin()) return;
    fengyin::SoundPreset preset;
    for (const auto& existing : cachedPresets)
        if ((editingPresetId.isNotEmpty() && existing.id == editingPresetId)
            || (editingPresetId.isEmpty() && existing.name.equalsIgnoreCase(name) && existing.pluginBrand == currentPluginBrand))
        {
            preset = existing;
            break;
        }
    if (preset.id.isEmpty()) preset.id = juce::Uuid().toString();
    preset.name = name;
    preset.pluginIdentifier = pluginHost.getPluginIdentifier();
    preset.toneParameters = pluginHost.captureToneParameters();
    preset.instrumentModelIndex = pluginHost.getCurrentInstrumentModelIndex();
    preset.pluginBrand = currentPluginBrand;
    preset.instrumentKey = currentInstrumentKey;
    preset.instrumentChineseName = currentInstrumentChineseName;
    preset.pluginProgramName = pluginHost.getCurrentProgramName();
    preset.customTone = true;
    preset.baseToneStyleId = baseStyleId;
    preset.toneStyleId = baseStyleId;
    const auto settings = masterOutput.getToneStyle();
    preset.eqTone = settings.tone;
    preset.warmth = settings.warmth;
    preset.reverbMix = settings.reverbMix;
    preset.compressionThreshold = settings.compressionThreshold;
    preset.compressionRatio = settings.compressionRatio;
    preset.saturation = settings.saturation;
    preset.harshControl = settings.harshControl;
    preset.reverbRoomSize = settings.reverbRoomSize;
    preset.reverbDamping = settings.reverbDamping;
    preset.reverbWidth = settings.reverbWidth;
    preset.outputGain = settings.outputGain;
    preset.bass = settings.bass;
    if (presetStore.save(preset))
    {
        currentPresetDisplayName = name;
        currentPresetId = preset.id;
        currentPresetIsCustom = true;
        currentBaseToneSettings = settings;
        editingPresetId.clear();
        editingReturnValid = false;
        refreshPresetChoices();
        pluginStatus.setText(utf8("已保存我的方案：") + name, juce::dontSendNotification);
    }
    else
        pluginStatus.setText(utf8("保存失败，请检查磁盘空间"), juce::dontSendNotification);
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
                if (safe->currentPresetId == id)
                {
                    const auto fallback = fengyin::ToneStyleCatalog::find(safe->currentInstrumentKey,
                                                                          safe->currentToneStyleId.startsWith("custom:")
                                                                              ? "natural" : safe->currentToneStyleId);
                    safe->currentPresetId.clear();
                    safe->currentPresetDisplayName.clear();
                    safe->currentPresetIsCustom = false;
                    safe->currentToneStyleId = fallback.id;
                    safe->currentBaseToneSettings = fallback.settings;
                    safe->masterOutput.setToneStyle(fallback.settings);
                    safe->currentSwamToneParameterCount = safe->currentPluginBrand == "swam"
                        ? safe->pluginHost.applySwamToneProfile(fallback.swam) : 0;
                }
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
    const auto licenseReady = license.getStatus().canUseFeatures();
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
        {}, utf8("您可以先开始3天完整试用，满意后再永久激活。"),
        utf8("请连接并打开电吹管，然后运行连接向导。"),
        utf8("请检查耳机或音响。风吟会自动选择 Windows 共享低延迟方案。"),
        utf8("最后扫描电脑中的 SWAM/VST3 音源。首次扫描可能需要一些时间。"),
        utf8("基础设置已经完成，可以开始演奏。")
    };
    message += instructions[nextStep];
    setupGuideDialog = std::make_unique<juce::AlertWindow>(automatic ? utf8("欢迎使用风吟") : utf8("风吟使用向导"), message,
                                                            nextStep == 5 ? juce::MessageBoxIconType::InfoIcon
                                                                          : juce::MessageBoxIconType::QuestionIcon);
    const juce::String buttonLabels[] { {}, utf8("试用或激活"), utf8("连接电吹管"), utf8("声音设置"), utf8("扫描音源"), utf8("完成") };
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

void MainComponent::captureToneBeforePresetEdit()
{
    editingReturnValid = pluginHost.hasPlugin();
    if (! editingReturnValid) return;
    editingReturnPluginIdentifier = pluginHost.getPluginIdentifier();
    editingReturnPresetId = currentPresetId;
    editingReturnPresetName = currentPresetDisplayName;
    editingReturnStyleId = currentToneStyleId;
    editingReturnPluginBrand = currentPluginBrand;
    editingReturnInstrumentKey = currentInstrumentKey;
    editingReturnInstrumentName = currentInstrumentChineseName;
    editingReturnProgramName = pluginHost.getCurrentProgramName();
    editingReturnWasCustom = currentPresetIsCustom;
    editingReturnModelIndex = pluginHost.getCurrentInstrumentModelIndex();
    editingReturnToneParameters = pluginHost.captureToneParameters();
    editingReturnToneSettings = masterOutput.getToneStyle();
}

void MainComponent::restoreToneBeforePresetEdit()
{
    if (! editingReturnValid) return;
    editingReturnValid = false;

    juce::PluginDescription chosen;
    bool found = false;
    for (const auto& candidate : pluginCatalog.getPlugins())
        if (candidate.createIdentifierString() == editingReturnPluginIdentifier)
        {
            chosen = candidate;
            found = true;
            break;
        }
    if (! found) return;

    const auto status = audio.getStatus();
    const auto presetId = editingReturnPresetId;
    const auto presetName = editingReturnPresetName;
    const auto styleId = editingReturnStyleId;
    const auto brand = editingReturnPluginBrand;
    const auto instrumentKey = editingReturnInstrumentKey;
    const auto instrumentName = editingReturnInstrumentName;
    const auto programName = editingReturnProgramName;
    const auto wasCustom = editingReturnWasCustom;
    const auto modelIndex = editingReturnModelIndex;
    const auto parameters = editingReturnToneParameters;
    const auto toneSettings = editingReturnToneSettings;
    pluginLoading = true;
    pluginHost.loadAsync(chosen,
                         status.sampleRate > 0.0 ? status.sampleRate : 48000.0,
                         status.bufferSize > 0 ? status.bufferSize : 128,
                         [this, presetId, presetName, styleId, brand, instrumentKey, instrumentName,
                          programName, wasCustom, modelIndex, parameters, toneSettings]
                         (bool success, const juce::String& message)
                         {
                             pluginLoading = false;
                             if (! success) return;
                             currentPluginBrand = brand;
                             currentInstrumentKey = instrumentKey;
                             currentInstrumentChineseName = instrumentName;
                             activatePluginOutput(message);
                             if (brand == "kong" && programName.isNotEmpty())
                                 pluginHost.selectProgramByAliases({ programName });
                             if (modelIndex >= 0) pluginHost.selectInstrumentModel(modelIndex);
                             pluginHost.restoreToneParameters(parameters);
                             masterOutput.setToneStyle(toneSettings);
                             currentToneStyleId = styleId;
                             currentPresetId = presetId;
                             currentPresetDisplayName = presetName;
                             currentPresetIsCustom = wasCustom;
                             currentBaseToneSettings = toneSettings;
                             currentSwamToneParameterCount = parameters.size();
                             pluginStatus.setText(utf8("已取消修改，恢复原音色"), juce::dontSendNotification);
                         });
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
    if (! fengyin::SupportedInstrumentClassifier::isSupported(chosen.name, chosen.manufacturerName))
    {
        const auto message = utf8("当前版本尚未支持 Kontakt、三体等其他音源，请关注后续版本升级。");
        pluginStatus.setText(message, juce::dontSendNotification);
        if (completion) completion(false, message);
        return;
    }

    const auto status = audio.getStatus();
    currentPluginBrand = preset.pluginBrand;
    currentInstrumentKey = preset.instrumentKey;
    currentInstrumentChineseName = preset.instrumentChineseName;
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
                             activatePluginOutput(message);
                             if (preset.pluginBrand == "kong" && preset.pluginProgramName.isNotEmpty())
                                 pluginHost.selectProgramByAliases({ preset.pluginProgramName });
                             auto style = fengyin::ToneStyleCatalog::find(currentInstrumentKey, preset.baseToneStyleId);
                             style.settings.tone = preset.eqTone;
                             style.settings.warmth = preset.warmth;
                             style.settings.reverbMix = preset.reverbMix;
                             if (preset.customTone)
                             {
                                 style.settings.compressionThreshold = preset.compressionThreshold;
                                 style.settings.compressionRatio = preset.compressionRatio;
                                 style.settings.saturation = preset.saturation;
                                 style.settings.harshControl = preset.harshControl;
                                 style.settings.reverbRoomSize = preset.reverbRoomSize;
                                 style.settings.reverbDamping = preset.reverbDamping;
                                 style.settings.reverbWidth = preset.reverbWidth;
                                 style.settings.outputGain = preset.outputGain;
                                 style.settings.bass = preset.bass;
                             }
                             if (preset.instrumentModelIndex >= 0)
                                 pluginHost.selectInstrumentModel(preset.instrumentModelIndex);
                             pluginHost.restoreToneParameters(preset.toneParameters);
                             currentToneStyleId = "custom:" + preset.id;
                             currentBaseToneSettings = style.settings;
                             masterOutput.setToneStyle(style.settings);
                             currentSwamToneParameterCount = preset.toneParameters.size();
                             currentPresetDisplayName = preset.name;
                             currentPresetId = preset.id;
                             currentPresetIsCustom = preset.customTone;
                             pluginStatus.setText(utf8("已恢复音色：") + preset.name, juce::dontSendNotification);
                             pluginHost.unloadEffect();
                             effectStatus.setText(utf8("内置音色引擎已启用"), juce::dontSendNotification);
                             if (completion) completion(true, utf8("音色方案已载入"));
                         });
}

void MainComponent::activatePluginOutput(const juce::String& pluginName)
{
    audio.getDeviceManager().removeAudioCallback(&testSynth);
    pluginHost.attachTo(audio.getDeviceManager());
    const auto brand = fengyin::SupportedInstrumentClassifier::classify(pluginName, {});
    const auto family = brand == fengyin::SupportedInstrumentClassifier::Brand::swam
        ? fengyin::SwamPluginClassifier::classify(pluginName.toStdString(), {}) : fengyin::SwamFamily::notSwam;
    if (brand == fengyin::SupportedInstrumentClassifier::Brand::kong) currentPluginBrand = "kong";
    else if (brand == fengyin::SupportedInstrumentClassifier::Brand::swam) currentPluginBrand = "swam";
    else if (currentPluginBrand.isEmpty()) currentPluginBrand = "swam";
    if (currentPluginBrand == "swam" || currentInstrumentKey.isEmpty() || ! currentInstrumentKey.startsWith("kong-"))
    {
        currentInstrumentKey = currentPluginBrand == "swam"
            ? utf8(fengyin::SwamPluginClassifier::instrumentKey(pluginName.toStdString())) : "kong-engine";
        currentInstrumentChineseName = currentPluginBrand == "swam"
            ? utf8(fengyin::SwamPluginClassifier::instrumentChineseName(pluginName.toStdString())) : utf8("空音 Qin Engine");
    }
    pluginHost.setKongExpressionMode(currentPluginBrand == "kong");
    midi.setTechniqueContext(fengyin::SwamPluginClassifier::familyKey(family));
    midi.setPerformanceSink(&pluginHost);
    configureTechniqueDefaults();
    if (currentPluginBrand == "kong")
    {
        if (const auto* definition = fengyin::KongInstrumentCatalog::find(currentInstrumentKey))
            masterOutput.setInstrumentProfile(definition->profile);
        else
            masterOutput.setInstrumentProfile(fengyin::InstrumentMixProfile::generic);
    }
    else switch (family)
    {
        case fengyin::SwamFamily::saxophone: masterOutput.setInstrumentProfile(fengyin::InstrumentMixProfile::saxophone); break;
        case fengyin::SwamFamily::brass:     masterOutput.setInstrumentProfile(fengyin::InstrumentMixProfile::brass); break;
        case fengyin::SwamFamily::woodwind:  masterOutput.setInstrumentProfile(fengyin::InstrumentMixProfile::woodwind); break;
        case fengyin::SwamFamily::strings:   masterOutput.setInstrumentProfile(fengyin::InstrumentMixProfile::strings); break;
        case fengyin::SwamFamily::notSwam:
        case fengyin::SwamFamily::other:     masterOutput.setInstrumentProfile(fengyin::InstrumentMixProfile::generic); break;
    }
    const auto defaultStyle = fengyin::ToneStyleCatalog::find(currentInstrumentKey, "natural");
    currentToneStyleId = defaultStyle.id;
    currentBaseToneSettings = defaultStyle.settings;
    masterOutput.setToneStyle(defaultStyle.settings);
    currentSwamToneParameterCount = currentPluginBrand == "swam"
        ? pluginHost.applySwamToneProfile(defaultStyle.swam) : 0;
    pluginStatus.setText(utf8("当前音源：") + pluginName, juce::dontSendNotification);
    if (! pluginHost.hasEffect())
        effectStatus.setText(utf8("效果器：未使用"), juce::dontSendNotification);
}

void MainComponent::applyCurrentSwamToneStyle()
{
    if (currentPluginBrand != "swam" || ! pluginHost.hasPlugin())
    {
        currentSwamToneParameterCount = 0;
        return;
    }
    const auto style = fengyin::ToneStyleCatalog::find(currentInstrumentKey, currentToneStyleId);
    currentSwamToneParameterCount = pluginHost.applySwamToneProfile(style.swam);
}

void MainComponent::configureTechniqueDefaults()
{
    if (! pluginHost.hasPlugin() || ! midi.getTechniqueMappings().isEmpty()) return;
    juce::Array<fengyin::TechniqueMapping> defaults;
    for (const auto [role, strength] : {
            std::pair { fengyin::PerformanceTechnique::vibrato, 0.55f },
            std::pair { fengyin::PerformanceTechnique::growl, 0.50f },
            std::pair { fengyin::PerformanceTechnique::portamento, 0.45f },
            std::pair { fengyin::PerformanceTechnique::mute, 0.50f } })
    {
        fengyin::TechniqueMapping mapping;
        mapping.technique = role;
        mapping.mode = role == fengyin::PerformanceTechnique::mute
            ? fengyin::TechniqueControlMode::hardware : fengyin::TechniqueControlMode::breath;
        mapping.strength = strength;
        defaults.add(mapping);
    }
    midi.setTechniqueMappings(defaults);
}

fengyin::SwamFamily MainComponent::currentTechniqueFamily() const
{
    if (! pluginHost.hasPlugin()) return fengyin::SwamFamily::notSwam;
    if (currentPluginBrand != "kong")
        return fengyin::SwamPluginClassifier::classify(pluginHost.getPluginName().toStdString(), {});

    if (const auto* definition = fengyin::KongInstrumentCatalog::find(currentInstrumentKey))
        switch (definition->profile)
        {
            case fengyin::InstrumentMixProfile::saxophone: return fengyin::SwamFamily::saxophone;
            case fengyin::InstrumentMixProfile::strings:  return fengyin::SwamFamily::strings;
            case fengyin::InstrumentMixProfile::brass:    return fengyin::SwamFamily::brass;
            case fengyin::InstrumentMixProfile::woodwind: return fengyin::SwamFamily::woodwind;
            case fengyin::InstrumentMixProfile::generic:  break;
        }
    return fengyin::SwamFamily::other;
}

void MainComponent::refreshLicenseUi()
{
    currentLicenseStatus = license.getStatus();
    isPermanent = currentLicenseStatus.activated;
    isActivated = currentLicenseStatus.canUseFeatures();

    if (isPermanent)
        licenseButton.setButtonText(utf8("✓ 永久版"));
    else if (currentLicenseStatus.trialActive)
    {
        const auto hours = juce::jmax<juce::int64>(1, (currentLicenseStatus.trialRemainingSeconds + 3599) / 3600);
        licenseButton.setButtonText(utf8("试用剩余 ") + juce::String(hours) + utf8(" 小时"));
    }
    else if (currentLicenseStatus.trialExpired)
        licenseButton.setButtonText(utf8("试用已到期 · 去激活"));
    else
        licenseButton.setButtonText(utf8("开始3天试用"));
    licenseButton.setColour(juce::TextButton::buttonColourId,
                            isPermanent ? juce::Colour(0xff176b57)
                                        : currentLicenseStatus.trialActive ? juce::Colour(0xff17617a)
                                                                           : juce::Colour(0xff7a4d16));

    if (featureAudioEnabled != isActivated)
    {
        if (! isActivated)
        {
            if (recorder.isRecording()) recorder.stop();
            accompaniment.pause();
            videoPlaybackActive = false;
            midi.setPerformanceSink(nullptr);
            pluginHost.detach();
            audio.getDeviceManager().removeAudioCallback(&testSynth);
        }
        else if (pluginHost.hasPlugin())
            activatePluginOutput(pluginHost.getPluginName());
        else
        {
            audio.getDeviceManager().addAudioCallback(&testSynth);
            midi.setPerformanceSink(&testSynth);
        }
        featureAudioEnabled = isActivated;
    }
}

void MainComponent::emitLicenseState(const fengyin::LicenseStatus& status)
{
    if (webInterface == nullptr) return;
    auto result = std::make_unique<juce::DynamicObject>();
    result->setProperty("activated", status.activated);
    result->setProperty("featuresUnlocked", status.canUseFeatures());
    result->setProperty("trialActive", status.trialActive);
    result->setProperty("trialExpired", status.trialExpired);
    result->setProperty("trialRemainingSeconds", status.trialRemainingSeconds);
    result->setProperty("message", status.message);
    webInterface->emitEventIfBrowserIsVisible("licenseStateResult", juce::var(result.release()));
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

    auto explanation = current.trialExpired
        ? utf8("3天完整试用已经结束。请输入永久激活码后继续使用。")
        : current.trialActive
            ? utf8("当前正在完整试用。您也可以随时输入永久激活码。")
            : utf8("您可以立即开始3天完整试用，或输入永久激活码。试用开始后会连续计算72小时。 ");
    activationDialog = std::make_unique<juce::AlertWindow>(utf8("试用与激活风吟"),
        explanation + utf8("\n\n本机码：") + license.getMachineCode(),
        juce::MessageBoxIconType::QuestionIcon);
    activationDialog->addTextEditor("code", {}, utf8("唯一激活码"));
    activationDialog->addButton(utf8("确认激活"), 1, juce::KeyPress(juce::KeyPress::returnKey));
    if (! current.trialActive && ! current.trialExpired)
        activationDialog->addButton(utf8("开始3天完整试用"), 2);
    activationDialog->addButton(utf8("取消"), 0, juce::KeyPress(juce::KeyPress::escapeKey));
    activationDialog->enterModalState(true, juce::ModalCallbackFunction::create([this](int result)
    {
        if (result == 1 && activationDialog != nullptr)
        {
            const auto status = license.activate(activationDialog->getTextEditorContents("code"));
            refreshLicenseUi();
            emitLicenseState(status);
            juce::AlertWindow::showMessageBoxAsync(status.activated ? juce::MessageBoxIconType::InfoIcon
                                                                    : juce::MessageBoxIconType::WarningIcon,
                                                   status.activated ? utf8("激活成功") : utf8("无法激活"), status.message);
        }
        else if (result == 2)
        {
            const auto status = license.startTrial();
            refreshLicenseUi();
            emitLicenseState(status);
            juce::AlertWindow::showMessageBoxAsync(status.trialActive ? juce::MessageBoxIconType::InfoIcon
                                                                       : juce::MessageBoxIconType::WarningIcon,
                                                   status.trialActive ? utf8("完整试用已开始") : utf8("无法开始试用"),
                                                   status.trialActive ? utf8("从现在起72小时内，风吟全部功能均可使用。")
                                                                      : status.message);
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
    result->setProperty("xruns", status.xRunCount);
    result->setProperty("cpuUsage", status.cpuUsage);
    result->setProperty("droppedMidiEvents", static_cast<juce::int64>(pluginHost.getDroppedMidiEventCount()));
    result->setProperty("automatic", audio.isAutomaticMode());
    result->setProperty("autoTuning", audio.isAutomaticLatencyTuning());
    result->setProperty("tuningProgress", audio.getTuningProgress());
    result->setProperty("callbackOverruns", static_cast<juce::int64>(pluginHost.getCallbackOverruns()));
    result->setProperty("lowLatencyMode", status.deviceType.containsIgnoreCase("Low Latency Mode")
                                              || status.deviceType.containsIgnoreCase(utf8("低延迟")));

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
    pluginHost.setLatencyProbeActive(false);
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
        utf8("推荐使用自动优化。所有可选模式均为 Windows 共享输出，不影响其他软件发声。"),
        juce::MessageBoxIconType::QuestionIcon);
    deviceDialog->addComboBox("type", types, utf8("声音驱动"));
    auto* typeBox = deviceDialog->getComboBoxComponent("type");
    typeBox->setText(current.deviceType, juce::dontSendNotification);
    deviceDialog->addComboBox("output", audio.getAvailableOutputDevices(current.deviceType), utf8("输出设备"));
    auto* outputBox = deviceDialog->getComboBoxComponent("output");
    outputBox->setText(current.deviceName, juce::dontSendNotification);
    juce::StringArray rateLabels;
    int selectedRate = 1;
    const auto availableRates = audio.getAvailableSampleRates();
    for (int index = 0; index < availableRates.size(); ++index)
    {
        const auto rate = juce::roundToInt(availableRates[index]);
        rateLabels.add(juce::String(rate) + (rate == 48000 ? utf8(" Hz（推荐）") : " Hz"));
        if (std::abs(availableRates[index] - current.sampleRate) < 1.0) selectedRate = index + 1;
    }
    if (rateLabels.isEmpty()) rateLabels.add(juce::String(juce::roundToInt(current.sampleRate)) + " Hz");
    deviceDialog->addComboBox("rate", rateLabels, utf8("采样率"));
    auto* rateBox = deviceDialog->getComboBoxComponent("rate");
    rateBox->setSelectedId(selectedRate, juce::dontSendNotification);
    juce::StringArray bufferLabels;
    int selectedBuffer = 1;
    const auto availableBuffers = audio.getAvailableBufferSizes();
    for (int index = 0; index < availableBuffers.size(); ++index)
    {
        const auto size = availableBuffers[index];
        bufferLabels.add(juce::String(size) + (size == 128 ? utf8("（低延迟）")
                                               : size == 256 ? utf8("（更稳定）") : juce::String()));
        if (size == current.bufferSize) selectedBuffer = index + 1;
    }
    if (bufferLabels.isEmpty()) bufferLabels.add(juce::String(current.bufferSize));
    deviceDialog->addComboBox("buffer", bufferLabels, utf8("共享缓冲周期"));
    auto* bufferBox = deviceDialog->getComboBoxComponent("buffer");
    bufferBox->setSelectedId(selectedBuffer, juce::dontSendNotification);
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
            auto error = audio.selectDeviceType(type);
            if (error.isEmpty())
            {
                const auto rate = deviceDialog->getComboBoxComponent("rate")->getText().getDoubleValue();
                const auto buffer = deviceDialog->getComboBoxComponent("buffer")->getText().getIntValue();
                error = audio.applyOutputSetup(output, rate, buffer);
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
        utf8("推荐先使用“自然、均衡”。气息抖动时调得更稳定，轻吹不响时调得更灵敏。弯音信号由风吟原样传给音源。"),
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
                const auto selected = [this](const char* name)
                { return juce::jlimit(1, 3, expressionDialog->getComboBoxComponent(name)->getSelectedId()) - 1; };
                next = { thresholds[selected("threshold")], curves[selected("response")],
                         smoothing[selected("smooth")], 1.0f };
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
