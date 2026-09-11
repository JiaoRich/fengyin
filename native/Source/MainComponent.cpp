#include "MainComponent.h"
#include <algorithm>

namespace
{
const juce::Colour background { 0xff07101d };
const juce::Colour panel { 0xff102238 };
const juce::Colour cyan { 0xff43d9ff };
const juce::Colour violet { 0xff9b63ff };
juce::String utf8(const char* text) { return juce::String::fromUTF8(text); }
const auto licensePublicKey = juce::String("5,d47f8d2272ed935eb504695cc78aa24a67e8b7a006c2e62e31c047727e7963e836cc0e36bf528b328e4eb0e73cacdc7a52160c961027a7fc7c77195d8a8e3568ca07e82303a9cb256b5627ea1ad8815a224801576ac89b548030474b1743f7e067cbd2ade4cc983a1973ca0a775b4c7c84e863ddf5cabb49ab5d684b8672fdad");
}

MainComponent::MainComponent() : license(licensePublicKey)
{
    setOpaque(true);
    setSize(1280, 800);

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
    startTimerHz(30);
    autoGuideAtMs = juce::Time::getMillisecondCounterHiRes() + 900.0;
}

MainComponent::~MainComponent()
{
    recorder.stop();
    midi.setPerformanceSink(nullptr);
    pluginHost.detach();
    audio.getDeviceManager().removeAudioCallback(&testSynth);
}

void MainComponent::paint(juce::Graphics& g)
{
    g.fillAll(background);

    auto area = getLocalBounds().toFloat().reduced(28.0f);
    auto stage = area.withTrimmedTop(78.0f).withTrimmedBottom(185.0f);
    g.setColour(panel);
    g.fillRoundedRectangle(stage, 22.0f);

    stage.removeFromTop(104.0f);
    const auto centre = stage.removeFromRight(350.0f).getCentre();
    const auto breath = juce::jlimit(0.0f, 1.0f, snapshot.breath);
    const auto radius = 105.0f + breath * 18.0f;
    g.setColour(cyan.withAlpha(0.18f + breath * 0.35f));
    g.fillEllipse(centre.x - radius, centre.y - radius, radius * 2.0f, radius * 2.0f);
    g.setColour(cyan);
    g.drawEllipse(centre.x - radius, centre.y - radius, radius * 2.0f, radius * 2.0f, 2.0f);
    g.setColour(violet.withAlpha(0.75f));
    g.drawEllipse(centre.x - radius + 17.0f, centre.y - radius + 17.0f,
                  radius * 2.0f - 34.0f, radius * 2.0f - 34.0f, 1.0f);

    auto visual = area.removeFromBottom(160.0f);
    g.setColour(panel);
    g.fillRoundedRectangle(visual, 18.0f);
    g.setGradientFill(juce::ColourGradient(cyan, visual.getX(), visual.getBottom(),
                                           violet, visual.getRight(), visual.getY(), false));
    const auto bars = 54;
    auto meterArea = visual.removeFromRight(34.0f).reduced(7.0f, 12.0f);
    const auto meterWidth = 7.0f;
    g.setColour(juce::Colour(0xff203346));
    g.fillRoundedRectangle(meterArea.getX(), meterArea.getY(), meterWidth, meterArea.getHeight(), 3.0f);
    g.fillRoundedRectangle(meterArea.getRight() - meterWidth, meterArea.getY(), meterWidth, meterArea.getHeight(), 3.0f);
    g.setColour(displayedLeftPeak > 0.9f || displayedRightPeak > 0.9f ? juce::Colour(0xffff526b) : cyan);
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
}

void MainComponent::resized()
{
    auto area = getLocalBounds().reduced(28);
    auto header = area.removeFromTop(68);
    title.setBounds(header.removeFromLeft(360));
    detectButton.setBounds(header.removeFromRight(110).reduced(4, 12));
    settingsButton.setBounds(header.removeFromRight(110).reduced(4, 12));
    expressionButton.setBounds(header.removeFromRight(110).reduced(4, 12));
    helpButton.setBounds(header.removeFromRight(98).reduced(4, 12));
    licenseButton.setBounds(header.removeFromRight(118).reduced(4, 12));
    audioStatus.setBounds(header.removeFromRight(220));
    deviceStatus.setBounds(header);

    auto stage = area.withTrimmedBottom(178);
    auto pluginRow = stage.removeFromTop(54).reduced(18, 7);
    scanPluginsButton.setBounds(pluginRow.removeFromLeft(116));
    pluginRow.removeFromLeft(8);
    loadPluginButton.setBounds(pluginRow.removeFromRight(116));
    pluginRow.removeFromRight(8);
    pluginEditorButton.setBounds(pluginRow.removeFromRight(106));
    pluginRow.removeFromRight(8);
    pluginSelector.setBounds(pluginRow.removeFromLeft(330));
    pluginRow.removeFromLeft(10);
    pluginStatus.setBounds(pluginRow);
    auto presetRow = stage.removeFromTop(50).reduced(18, 5);
    presetSelector.setBounds(presetRow.removeFromLeft(330));
    presetRow.removeFromLeft(8);
    loadPresetButton.setBounds(presetRow.removeFromLeft(110));
    presetRow.removeFromLeft(8);
    savePresetButton.setBounds(presetRow.removeFromLeft(140));
    presetRow.removeFromLeft(12);
    favoritePresetButton.setBounds(presetRow.removeFromLeft(76));
    presetRow.removeFromLeft(6);
    defaultPresetButton.setBounds(presetRow.removeFromLeft(94));
    presetRow.removeFromLeft(6);
    deletePresetButton.setBounds(presetRow.removeFromLeft(70));
    presetRow.removeFromLeft(10);
    recordButton.setBounds(presetRow.removeFromLeft(132));
    presetRow.removeFromLeft(8);
    recordingStatus.setBounds(presetRow);
    auto effectRow = stage.removeFromTop(48).reduced(18, 5);
    effectSelector.setBounds(effectRow.removeFromLeft(330));
    effectRow.removeFromLeft(8);
    loadEffectButton.setBounds(effectRow.removeFromLeft(110));
    effectRow.removeFromLeft(8);
    removeEffectButton.setBounds(effectRow.removeFromLeft(110));
    effectRow.removeFromLeft(10);
    effectEditorButton.setBounds(effectRow.removeFromLeft(104));
    effectRow.removeFromLeft(8);
    bypassEffectButton.setBounds(effectRow.removeFromLeft(88));
    effectRow.removeFromLeft(10);
    effectStatus.setBounds(effectRow);
    auto masterRow = stage.removeFromBottom(38).reduced(18, 2);
    masterVolumeLabel.setBounds(masterRow.removeFromLeft(66));
    masterVolume.setBounds(masterRow.removeFromLeft(290));
    recordingManagerButton.setBounds(masterRow.removeFromRight(110));
    auto performanceArea = stage.removeFromRight(350);
    videoPlayer.setBounds(stage.reduced(18, 8));
    noteLabel.setBounds(performanceArea.withSizeKeepingCentre(250, 110).translated(0, -14));
    breathLabel.setBounds(performanceArea.withSizeKeepingCentre(330, 38).translated(0, 70));
}

void MainComponent::timerCallback()
{
    snapshot = midi.getSnapshot();
    const auto currentAudio = audio.getStatus();
    const auto scanProgress = pluginCatalog.getProgress();
    simulatedPhase += 0.09f;
    displayedLeftPeak = juce::jmax(displayedLeftPeak * 0.88f, juce::jlimit(0.0f, 1.0f, masterOutput.getLeftPeak()));
    displayedRightPeak = juce::jmax(displayedRightPeak * 0.88f, juce::jlimit(0.0f, 1.0f, masterOutput.getRightPeak()));
    masterOutput.getSpectrum(spectrumLevels);
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
    for (const auto& plugin : swamPlugins)
        if (plugin.isInstrument)
        {
            cachedInstrumentPlugins.add(plugin);
            pluginSelector.addItem(utf8("SWAM · ") + plugin.name, itemId++);
        }
    for (const auto& plugin : plugins)
    {
        bool isSwam = false;
        for (const auto& swam : swamPlugins)
            if (swam.createIdentifierString() == plugin.createIdentifierString())
                isSwam = true;
        if (! isSwam && plugin.isInstrument)
        {
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
    loadPluginButton.setEnabled(false);
    pluginHost.loadAsync(chosen,
                         status.sampleRate > 0.0 ? status.sampleRate : 48000.0,
                         status.bufferSize > 0 ? status.bufferSize : 128,
                         [this](bool success, const juce::String& message)
                         {
                             loadPluginButton.setEnabled(true);
                             if (! success)
                             {
                                 useTestSynth();
                                 pluginStatus.setText(utf8("加载失败，已恢复测试音源：") + message,
                                                      juce::dontSendNotification);
                                 return;
                             }
                             activatePluginOutput(message);
                         });
}

void MainComponent::loadSelectedEffect()
{
    const auto index = effectSelector.getSelectedId() - 1;
    if (! juce::isPositiveAndBelow(index, cachedEffectPlugins.size()) || ! pluginHost.hasPlugin()) return;
    const auto chosen = cachedEffectPlugins.getReference(index);
    const auto status = audio.getStatus();
    effectStatus.setText(utf8("正在加载效果器：") + chosen.name, juce::dontSendNotification);
    loadEffectButton.setEnabled(false);
    pluginHost.loadEffectAsync(chosen, status.sampleRate > 0.0 ? status.sampleRate : 48000.0,
        status.bufferSize > 0 ? status.bufferSize : 128,
        [this](bool success, const juce::String& message)
        {
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

    savePresetDialog = std::make_unique<juce::AlertWindow>(utf8("保存音色方案"), utf8("给这套音源和效果器起一个容易记住的名字。"),
                                                           juce::MessageBoxIconType::QuestionIcon);
    savePresetDialog->addTextEditor("name", pluginHost.getPluginName() + utf8(" · 我的音色"), utf8("方案名称"));
    savePresetDialog->addButton(utf8("保存"), 1, juce::KeyPress(juce::KeyPress::returnKey));
    savePresetDialog->addButton(utf8("取消"), 0, juce::KeyPress(juce::KeyPress::escapeKey));
    savePresetDialog->enterModalState(true, juce::ModalCallbackFunction::create([this](int result)
    {
        if (result == 1 && savePresetDialog != nullptr)
            commitCurrentPreset(savePresetDialog->getTextEditorContents("name").trim());
        savePresetDialog.reset();
    }), false);
}

void MainComponent::commitCurrentPreset(const juce::String& name)
{
    if (name.isEmpty()) return;
    fengyin::SoundPreset preset;
    preset.id = juce::Uuid().toString();
    preset.name = name;
    preset.pluginIdentifier = pluginHost.getPluginIdentifier();
    preset.pluginState = pluginHost.savePluginState();
    preset.effectIdentifier = pluginHost.getEffectIdentifier();
    preset.effectState = pluginHost.saveEffectState();
    preset.effectBypassed = pluginHost.isEffectBypassed();
    preset.breathController = midi.getActiveProfile().breathController;
    preset.breathCurve = midi.getActiveProfile().breathCurve;
    preset.breathSmoothing = midi.getActiveProfile().smoothing;
    preset.masterVolume = masterOutput.getGain();

    if (presetStore.save(preset))
    {
        refreshPresetChoices();
        for (int i = 0; i < cachedPresets.size(); ++i)
            if (cachedPresets.getReference(i).id == preset.id)
                presetSelector.setSelectedId(i + 1, juce::dontSendNotification);
        pluginStatus.setText(utf8("音色方案已保存"), juce::dontSendNotification);
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

void MainComponent::loadSelectedPreset()
{
    const auto index = presetSelector.getSelectedId() - 1;
    if (! juce::isPositiveAndBelow(index, cachedPresets.size()))
        return;
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
        pluginStatus.setText(utf8("找不到此方案需要的音源，请重新扫描"), juce::dontSendNotification);
        return;
    }

    const auto status = audio.getStatus();
    pluginStatus.setText(utf8("正在恢复音色方案……"), juce::dontSendNotification);
    pluginHost.loadAsync(chosen,
                         status.sampleRate > 0.0 ? status.sampleRate : 48000.0,
                         status.bufferSize > 0 ? status.bufferSize : 128,
                         [this, preset](bool success, const juce::String& message)
                         {
                             if (! success)
                             {
                                 useTestSynth();
                                 pluginStatus.setText(utf8("方案载入失败：") + message, juce::dontSendNotification);
                                 return;
                             }
                             pluginHost.restorePluginState(preset.pluginState.getData(), preset.pluginState.getSize());
                             midi.setBreathController(preset.breathController);
                             masterVolume.setValue(preset.masterVolume, juce::sendNotificationSync);
                             activatePluginOutput(message);
                             pluginStatus.setText(utf8("已恢复音色：") + preset.name, juce::dontSendNotification);
                             if (preset.effectIdentifier.isEmpty())
                             {
                                 effectStatus.setText(utf8("效果器：未使用"), juce::dontSendNotification);
                                 return;
                             }
                             juce::PluginDescription effect;
                             bool foundEffect = false;
                             for (const auto& candidate : pluginCatalog.getPlugins())
                                 if (candidate.createIdentifierString() == preset.effectIdentifier)
                                 { effect = candidate; foundEffect = true; break; }
                             if (! foundEffect)
                             {
                                 effectStatus.setText(utf8("找不到方案中的效果器，音源已正常恢复"), juce::dontSendNotification);
                                 return;
                             }
                             const auto audioStatusNow = audio.getStatus();
                             pluginHost.loadEffectAsync(effect,
                                 audioStatusNow.sampleRate > 0.0 ? audioStatusNow.sampleRate : 48000.0,
                                 audioStatusNow.bufferSize > 0 ? audioStatusNow.bufferSize : 128,
                                 [this, preset](bool effectLoaded, const juce::String& effectMessage)
                                 {
                                     if (effectLoaded)
                                     {
                                         pluginHost.restoreEffectState(preset.effectState.getData(), preset.effectState.getSize());
                                         pluginHost.setEffectBypassed(preset.effectBypassed);
                                         bypassEffectButton.setToggleState(preset.effectBypassed, juce::dontSendNotification);
                                         bypassEffectButton.setButtonText(preset.effectBypassed ? utf8("已旁通") : utf8("旁通"));
                                         effectStatus.setText(utf8("已恢复效果器：") + effectMessage, juce::dontSendNotification);
                                     }
                                     else
                                         effectStatus.setText(utf8("效果器恢复失败：") + effectMessage, juce::dontSendNotification);
                                 });
                         });
}

void MainComponent::activatePluginOutput(const juce::String& pluginName)
{
    audio.getDeviceManager().removeAudioCallback(&testSynth);
    pluginHost.attachTo(audio.getDeviceManager());
    midi.setPerformanceSink(&pluginHost);
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
    if (! recorder.start(status.sampleRate, juce::jmax(1, channels)))
    {
        recordingStatus.setText(recorder.getLastError(), juce::dontSendNotification);
        return;
    }
    recordButton.setButtonText(utf8("■ 停止并保存"));
    recordButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xffa73545));
    recordingStatus.setText(utf8("正在录音  00:00"), juce::dontSendNotification);
}

void MainComponent::showDeviceSettings()
{
    const auto current = audio.getStatus();
    const auto types = audio.getAvailableDeviceTypes();
    deviceDialog = std::make_unique<juce::AlertWindow>(utf8("声音设备设置"),
        utf8("推荐：优先选择 ASIO，采样率 48000 Hz，缓冲区 128。若出现爆音，可改为 256。"),
        juce::MessageBoxIconType::QuestionIcon);
    deviceDialog->addComboBox("type", types, utf8("声音驱动"));
    auto* typeBox = deviceDialog->getComboBoxComponent("type");
    typeBox->setText(current.deviceType, juce::dontSendNotification);
    deviceDialog->addComboBox("output", audio.getAvailableOutputDevices(current.deviceType), utf8("输出设备"));
    auto* outputBox = deviceDialog->getComboBoxComponent("output");
    outputBox->setText(current.deviceName, juce::dontSendNotification);
    deviceDialog->addComboBox("rate", { "44100 Hz", "48000 Hz（推荐）", "96000 Hz" }, utf8("采样率"));
    auto* rateBox = deviceDialog->getComboBoxComponent("rate");
    rateBox->setSelectedId(current.sampleRate >= 88000.0 ? 3 : (current.sampleRate >= 46000.0 ? 2 : 1), juce::dontSendNotification);
    deviceDialog->addComboBox("buffer", { "64（低延迟）", "128（推荐）", "256（更稳定）", "512（最稳定）" }, utf8("缓冲区"));
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
    midiDialog->addComboBox("breathCC", { "CC2（大多数电吹管）", "CC11（表情）", "CC1（调制）" }, utf8("气息控制器"));
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
    expressionDialog->addComboBox("response", { "灵敏（轻吹更容易响）", "自然（推荐）", "稳重（强吹变化更明显）" }, utf8("气息响应"));
    expressionDialog->getComboBoxComponent("response")->setSelectedId(current.curve < 0.8f ? 1 : current.curve > 1.2f ? 3 : 2,
                                                                        juce::dontSendNotification);
    expressionDialog->addComboBox("smooth", { "快速（变化最灵敏）", "均衡（推荐）", "稳定（减少气息抖动）" }, utf8("平滑程度"));
    expressionDialog->getComboBoxComponent("smooth")->setSelectedId(current.smoothing > 0.38f ? 1 : current.smoothing < 0.2f ? 3 : 2,
                                                                      juce::dontSendNotification);
    expressionDialog->addComboBox("threshold", { "灵敏（适合轻吹）", "均衡（推荐）", "防误触（过滤微弱气流）" }, utf8("起音门槛"));
    expressionDialog->getComboBoxComponent("threshold")->setSelectedId(current.threshold < 0.018f ? 1 : current.threshold > 0.045f ? 3 : 2,
                                                                         juce::dontSendNotification);
    expressionDialog->addComboBox("pitch", { "轻柔（弯音幅度较小）", "标准（推荐）", "宽广（弯音幅度更大）" }, utf8("弯音灵敏度"));
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
