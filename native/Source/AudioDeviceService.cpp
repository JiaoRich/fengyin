#include "AudioDeviceService.h"
#include <algorithm>

namespace
{
constexpr int currentAudioSetupRevision = 7;
constexpr double latencyCandidateTestMs = 3500.0;

bool isWindowsSharedType(const juce::String& typeName)
{
   #if JUCE_WINDOWS
    return typeName == "Windows Audio"
        || typeName.containsIgnoreCase("Low Latency Mode")
        || typeName.containsIgnoreCase(juce::String::fromUTF8("低延迟"));
   #else
    return ! typeName.containsIgnoreCase("Exclusive") && ! typeName.containsIgnoreCase("ASIO");
   #endif
}

juce::Array<int> sortedLegalBuffers(juce::Array<int> sizes)
{
    sizes.removeAllInstancesOf(0);
    std::sort(sizes.begin(), sizes.end());
    for (int index = sizes.size() - 1; index > 0; --index)
        if (sizes[index] == sizes[index - 1]) sizes.remove(index);
    return sizes;
}

int minimumLegalBuffer(juce::Array<int> sizes, int driverDefault)
{
    sizes = sortedLegalBuffers(std::move(sizes));
    return sizes.isEmpty() ? driverDefault : sizes.getFirst();
}

double sharedMixRate(juce::AudioIODevice& device)
{
    const auto current = device.getCurrentSampleRate();
    const auto rates = device.getAvailableSampleRates();
    if (current > 0.0 && (rates.isEmpty() || rates.contains(current)))
        return current;
    if (rates.contains(48000.0))
        return 48000.0;
    return rates.isEmpty() ? 0.0 : rates.getFirst();
}
}

namespace fengyin
{
AudioDeviceService::AudioDeviceService()
{
    juce::PropertiesFile::Options options;
    options.applicationName = "FengYin";
    options.filenameSuffix = ".settings";
    options.folderName = "FengYin";
    options.osxLibrarySubFolder = "Application Support";
    properties.setStorageParameters(options);
}

juce::String AudioDeviceService::initialise()
{
    std::unique_ptr<juce::XmlElement> saved;
    if (auto* settings = properties.getUserSettings())
        saved = juce::parseXML(settings->getValue("audioDevice"));
   #if JUCE_WINDOWS
    // 旧版可能保存了 ASIO/Exclusive。启动时不允许短暂打开这些独占路径，
    // 而是直接从 Windows 共享设备起步。
    if (saved != nullptr && ! isWindowsSharedType(saved->getStringAttribute("deviceType")))
        saved.reset();
   #endif
    lastError = manager.initialise(0, 2, saved.get(), true);
    return lastError;
}

AudioDeviceStatus AudioDeviceService::getStatus()
{
    AudioDeviceStatus status;
    status.error = lastError;
    if (auto* device = manager.getCurrentAudioDevice())
    {
        status.ready = device->isOpen();
        status.deviceType = device->getTypeName();
        status.deviceName = device->getName();
        status.sampleRate = device->getCurrentSampleRate();
        status.bufferSize = device->getCurrentBufferSizeSamples();
        if (status.sampleRate > 0.0)
            status.estimatedBufferLatencyMs = (status.bufferSize + device->getOutputLatencyInSamples()) * 1000.0 / status.sampleRate;
        status.cpuUsage = manager.getCpuUsage();
        status.xRunCount = manager.getXRunCount();
    }
    return status;
}

juce::StringArray AudioDeviceService::getAvailableDeviceTypes()
{
    juce::StringArray names;
    for (const auto* type : manager.getAvailableDeviceTypes())
        if (isWindowsSharedType(type->getTypeName()))
            names.add(type->getTypeName());
    return names;
}

juce::StringArray AudioDeviceService::getAvailableOutputDevices(const juce::String& typeName)
{
    if (auto* type = findType(typeName))
    {
        type->scanForDevices();
        return type->getDeviceNames(false);
    }
    return {};
}

juce::Array<double> AudioDeviceService::getAvailableSampleRates()
{
    if (auto* device = manager.getCurrentAudioDevice())
        return device->getAvailableSampleRates();
    return {};
}

juce::Array<int> AudioDeviceService::getAvailableBufferSizes()
{
    if (auto* device = manager.getCurrentAudioDevice())
        return device->getAvailableBufferSizes();
    return {};
}

juce::String AudioDeviceService::selectDeviceType(const juce::String& typeName)
{
    if (! isWindowsSharedType(typeName))
        return juce::String::fromUTF8("风吟仅使用 Windows 共享输出，不会独占耳机或音响");
    manager.setCurrentAudioDeviceType(typeName, true);
    lastError = manager.getCurrentAudioDeviceType() == typeName
        ? juce::String()
        : juce::String::fromUTF8("无法启用所选声音驱动");
    if (lastError.isEmpty()) saveSettings();
    return lastError;
}

juce::String AudioDeviceService::applyOutputSetup(const juce::String& outputName,
                                                   double sampleRate,
                                                   int bufferSize)
{
    tuningActive = false;
    tuningCandidates.clear();
    tuningResults.clear();
    auto setup = manager.getAudioDeviceSetup();
    setup.outputDeviceName = outputName;
    setup.inputDeviceName.clear();
    setup.sampleRate = sampleRate;
    setup.bufferSize = bufferSize;
    setup.useDefaultInputChannels = false;
    setup.useDefaultOutputChannels = true;
    lastError = manager.setAudioDeviceSetup(setup, true);
    if (lastError.isEmpty())
    {
        if (auto* settings = properties.getUserSettings())
            settings->setValue("audioSetupMode", "manual");
        saveSettings();
    }
    return lastError;
}

juce::String AudioDeviceService::applyBestInitialSetup()
{
    auto* settings = properties.getUserSettings();
    const auto current = getStatus();
    const auto revision = settings != nullptr ? settings->getIntValue("audioSetupRevision", 0) : 0;
    // 升级后迁移旧版误判的固定大缓冲，并强制废弃任何已保存的
    // ASIO/独占路径；只保留用户亲自选择的 Windows 共享参数。
    const auto legacyFixedBuffer = revision < currentAudioSetupRevision
        && current.deviceType.equalsIgnoreCase("Windows Audio")
        && current.bufferSize >= 512;
    const auto legacyExclusiveMode = revision < currentAudioSetupRevision
        && ! isWindowsSharedType(current.deviceType);
    if (settings != nullptr && settings->getValue("audioSetupMode") == "manual"
        && ! legacyFixedBuffer && ! legacyExclusiveMode)
    {
        settings->setValue("audioSetupRevision", currentAudioSetupRevision);
        settings->saveIfNeeded();
        return juce::String::fromUTF8("已保留用户的声音设置");
    }

    auto preferredType = preferredLiveDeviceType();
    if (preferredType.isEmpty())
        preferredType = manager.getCurrentAudioDeviceType();

    // 不只看上次打开的设备：耳机、音响或 USB 声卡改变为系统默认输出后，
    // 自动模式在下次启动会为新设备重做一次安全配置。
    juce::String desiredSignature;
    if (auto* type = findType(preferredType))
    {
        type->scanForDevices();
        const auto outputs = type->getDeviceNames(false);
        const auto defaultIndex = type->getDefaultDeviceIndex(false);
        if (juce::isPositiveAndBelow(defaultIndex, outputs.size()))
            desiredSignature = preferredType + "|" + outputs[defaultIndex];
    }
    if (revision >= currentAudioSetupRevision && settings != nullptr && desiredSignature.isNotEmpty()
        && settings->getValue("automaticAudioDevice") == desiredSignature
        && currentDeviceSignature() == desiredSignature)
        return juce::String::fromUTF8("已恢复自动优化的声音设置");

    const auto fallbackType = manager.getCurrentAudioDeviceType();
    auto result = configureAutomaticType(preferredType);
    if (result.isNotEmpty() && fallbackType.isNotEmpty() && preferredType != fallbackType)
        result = configureAutomaticType(fallbackType);
    if (result.isNotEmpty())
        return result;

    if (settings != nullptr)
    {
        settings->setValue("audioSetupMode", "automatic");
        settings->setValue("automaticAudioDevice", currentDeviceSignature());
        settings->setValue("audioSetupRevision", currentAudioSetupRevision);
    }
    saveSettings();
    const auto status = getStatus();
    return juce::String::fromUTF8("已自动选择：") + status.deviceType + "、"
         + juce::String(juce::roundToInt(status.sampleRate)) + " Hz、"
         + juce::String(status.bufferSize) + juce::String::fromUTF8(" 采样");
}

juce::String AudioDeviceService::preferredLiveDeviceType()
{
    const auto types = getAvailableDeviceTypes();
   #if JUCE_WINDOWS
    for (const auto& type : types)
        if (type.containsIgnoreCase("Low Latency Mode")
            || type.containsIgnoreCase(juce::String::fromUTF8("低延迟")))
            return type;
   #endif
    return {};
}

juce::String AudioDeviceService::configureAutomaticType(const juce::String& typeName)
{
    if (typeName.isEmpty())
        return juce::String::fromUTF8("没有可用的声音驱动");

    manager.setCurrentAudioDeviceType(typeName, true);
    if (manager.getCurrentAudioDeviceType() != typeName)
        return juce::String::fromUTF8("无法启用声音驱动：") + typeName;

    auto* type = findType(typeName);
    if (type == nullptr)
        return juce::String::fromUTF8("无法读取声音驱动");
    type->scanForDevices();
    const auto outputs = type->getDeviceNames(false);
    const auto defaultIndex = type->getDefaultDeviceIndex(false);
    if (! juce::isPositiveAndBelow(defaultIndex, outputs.size()))
        return juce::String::fromUTF8("没有可用的声音输出设备");

    auto setup = manager.getAudioDeviceSetup();
    setup.outputDeviceName = outputs[defaultIndex];
    setup.inputDeviceName.clear();
    setup.useDefaultInputChannels = false;
    setup.useDefaultOutputChannels = true;

    // Open this endpoint with its own Windows shared defaults before asking it
    // for legal periods. Capabilities from the previously-open speaker are not
    // valid for a newly inserted headset or USB interface.
    setup.sampleRate = 0.0;
    setup.bufferSize = 0;
    lastError = manager.setAudioDeviceSetup(setup, true);
    if (lastError.isNotEmpty())
        return lastError;
    if (auto* device = manager.getCurrentAudioDevice())
    {
        setup = manager.getAudioDeviceSetup();
        setup.inputDeviceName.clear();
        setup.useDefaultInputChannels = false;
        setup.useDefaultOutputChannels = true;
        setup.sampleRate = sharedMixRate(*device);
        setup.bufferSize = minimumLegalBuffer(device->getAvailableBufferSizes(),
                                              device->getCurrentBufferSizeSamples());
        lastError = manager.setAudioDeviceSetup(setup, true);
    }
    return lastError;
}

juce::String AudioDeviceService::currentDeviceSignature() const
{
    if (auto* device = manager.getCurrentAudioDevice())
        return device->getTypeName() + "|" + device->getName();
    return {};
}

bool AudioDeviceService::followSystemDefaultOutput()
{
   #if JUCE_WINDOWS
    if (tuningActive || ! isAutomaticMode())
        return false;
    auto* current = manager.getCurrentAudioDevice();
    if (current == nullptr || ! isWindowsSharedType(current->getTypeName()))
        return false;

    auto* type = findType(manager.getCurrentAudioDeviceType());
    if (type == nullptr)
        return false;
    type->scanForDevices();
    const auto outputs = type->getDeviceNames(false);
    const auto defaultIndex = type->getDefaultDeviceIndex(false);
    if (! juce::isPositiveAndBelow(defaultIndex, outputs.size()))
        return false;

    const auto defaultOutput = outputs[defaultIndex];
    if (defaultOutput.isEmpty() || current->getName() == defaultOutput)
        return false;

    auto setup = manager.getAudioDeviceSetup();
    setup.outputDeviceName = defaultOutput;
    setup.inputDeviceName.clear();
    setup.useDefaultInputChannels = false;
    setup.useDefaultOutputChannels = true;
    lastError = manager.setAudioDeviceSetup(setup, true);
    if (lastError.isNotEmpty())
    {
        // 新端点可能不接受旧设备的采样率/周期。先用它自己的共享默认值打开，
        // 随后的静默预检再选出最低稳定周期。
        setup.sampleRate = 0.0;
        setup.bufferSize = 0;
        lastError = manager.setAudioDeviceSetup(setup, true);
    }
    if (lastError.isNotEmpty())
        return false;
    if (auto* settings = properties.getUserSettings())
    {
        settings->setValue("automaticAudioDevice", currentDeviceSignature());
        settings->removeValue("automaticLatencyTunedDevice");
    }
    saveSettings();
    return true;
   #else
    return false;
   #endif
}

bool AudioDeviceService::systemDefaultOutputChanged()
{
   #if JUCE_WINDOWS
    if (tuningActive || ! isAutomaticMode()) return false;
    auto* current = manager.getCurrentAudioDevice();
    if (current == nullptr || ! isWindowsSharedType(current->getTypeName())) return false;
    auto* type = findType(manager.getCurrentAudioDeviceType());
    if (type == nullptr) return false;
    type->scanForDevices();
    const auto outputs = type->getDeviceNames(false);
    const auto index = type->getDefaultDeviceIndex(false);
    return juce::isPositiveAndBelow(index, outputs.size())
        && outputs[index].isNotEmpty() && outputs[index] != current->getName();
   #else
    return false;
   #endif
}

juce::String AudioDeviceService::optimiseForLivePerformance()
{
    auto* device = manager.getCurrentAudioDevice();
    if (device == nullptr || ! device->isOpen())
        return juce::String::fromUTF8("声音设备尚未准备好");

   #if JUCE_WINDOWS
    // 优先进入 IAudioClient3 支持的 Windows 共享低延迟类型。
    {
        const auto preferredType = preferredLiveDeviceType();
        if (preferredType.isNotEmpty() && preferredType != manager.getCurrentAudioDeviceType())
        {
            if (const auto error = configureAutomaticType(preferredType); error.isNotEmpty())
                return error;
            device = manager.getCurrentAudioDevice();
            if (device == nullptr || ! device->isOpen())
                return juce::String::fromUTF8("低延迟声音驱动打开失败");
        }
    }
   #endif

    auto setup = manager.getAudioDeviceSetup();
    setup.inputDeviceName.clear();
    setup.useDefaultInputChannels = false;
    setup.useDefaultOutputChannels = true;

    setup.sampleRate = sharedMixRate(*device);
    setup.bufferSize = minimumLegalBuffer(device->getAvailableBufferSizes(),
                                          device->getCurrentBufferSizeSamples());

    lastError = manager.setAudioDeviceSetup(setup, true);
    observedXRunCount = manager.getXRunCount();
    unstablePolls = 0;
    if (lastError.isNotEmpty())
        return lastError;
    if (auto* settings = properties.getUserSettings())
    {
        settings->setValue("audioSetupMode", "automatic");
        settings->setValue("automaticAudioDevice", currentDeviceSignature());
        settings->setValue("audioSetupRevision", currentAudioSetupRevision);
    }
    saveSettings();
    const auto applied = getStatus();
    return juce::String::fromUTF8("实时演奏模式已启用：") + applied.deviceType + "、"
         + juce::String(juce::roundToInt(applied.sampleRate)) + " Hz、缓冲区 "
         + juce::String(applied.bufferSize) + juce::String::fromUTF8(" 采样");
}

bool AudioDeviceService::hasSustainedRuntimeInstability()
{
    if (tuningActive) return false;
    auto* device = manager.getCurrentAudioDevice();
    if (device == nullptr || ! device->isOpen()) return false;
    const auto xruns = manager.getXRunCount();
    if (xruns < 0) return false;
    const auto cpuOverloaded = manager.getCpuUsage() >= 0.82;
    if (xruns > observedXRunCount || cpuOverloaded)
        ++unstablePolls;
    else
        unstablePolls = 0;
    observedXRunCount = xruns;
    if (unstablePolls < 3) return false;
    unstablePolls = 0;
    return true;
}

bool AudioDeviceService::needsAutomaticLatencyTuning()
{
    auto* settings = properties.getUserSettings();
    if (settings == nullptr || settings->getValue("audioSetupMode") == "manual")
        return false;
    return settings->getIntValue("audioSetupRevision", 0) < currentAudioSetupRevision
        || settings->getValue("automaticLatencyTunedDevice") != currentDeviceSignature();
}

bool AudioDeviceService::isAutomaticMode()
{
    if (auto* settings = properties.getUserSettings())
        return settings->getValue("audioSetupMode") != "manual";
    return true;
}

void AudioDeviceService::buildLatencyCandidates()
{
    tuningCandidates.clear();
    auto add = [this](const juce::String& typeName, const juce::String& outputName,
                      int preferredBuffer, int priority)
    {
        if (typeName.isEmpty() || outputName.isEmpty()) return;
        for (const auto& existing : tuningCandidates)
            if (existing.typeName == typeName && existing.outputName == outputName
                && existing.preferredBuffer == preferredBuffer) return;
        tuningCandidates.push_back({ typeName, outputName, preferredBuffer, priority });
    };

    const auto current = getStatus();
    if (! current.ready) return;
    auto legal = sortedLegalBuffers(getAvailableBufferSizes());
    if (legal.isEmpty())
    {
        add(current.deviceType, current.deviceName, current.bufferSize, 0);
        return;
    }
    // Test only periods the active IAudioClient3 endpoint explicitly reports.
    // Never label a larger driver period as "128", and never add a hidden 512/1024 fallback.
    add(current.deviceType, current.deviceName, legal.getFirst(), 0);
    for (const auto size : legal)
        if (size > legal.getFirst() && size <= 256)
            add(current.deviceType, current.deviceName, size, 1);
}

bool AudioDeviceService::applyLatencyCandidate(const LatencyCandidate& candidate)
{
    manager.setCurrentAudioDeviceType(candidate.typeName, true);
    if (manager.getCurrentAudioDeviceType() != candidate.typeName)
        return false;

    auto setup = manager.getAudioDeviceSetup();
    setup.outputDeviceName = candidate.outputName;
    setup.inputDeviceName.clear();
    setup.useDefaultInputChannels = false;
    setup.useDefaultOutputChannels = true;
    if (auto* device = manager.getCurrentAudioDevice())
    {
        setup.sampleRate = sharedMixRate(*device);
        const auto legal = sortedLegalBuffers(device->getAvailableBufferSizes());
        if (! legal.isEmpty() && ! legal.contains(candidate.preferredBuffer))
            return false;
        setup.bufferSize = candidate.preferredBuffer;
    }
    else
    {
        setup.sampleRate = 48000.0;
        setup.bufferSize = candidate.preferredBuffer;
    }
    lastError = manager.setAudioDeviceSetup(setup, true);
    const auto status = getStatus();
    return lastError.isEmpty() && status.ready;
}

bool AudioDeviceService::startNextLatencyCandidate()
{
    while (++tuningCandidateIndex < static_cast<int>(tuningCandidates.size()))
    {
        if (! applyLatencyCandidate(tuningCandidates[static_cast<size_t>(tuningCandidateIndex)]))
            continue;
        tuningCandidateStartedAtMs = juce::Time::getMillisecondCounterHiRes();
        tuningCandidateStartXRuns = juce::jmax(0, manager.getXRunCount());
        tuningCandidateMaximumCpu = manager.getCpuUsage();
        return true;
    }
    return false;
}

bool AudioDeviceService::beginAutomaticLatencyTuning(bool force)
{
    if (tuningActive) return false;
    auto* settings = properties.getUserSettings();
    if (! force && ! needsAutomaticLatencyTuning()) return false;
    if (force && settings != nullptr)
        settings->setValue("audioSetupMode", "automatic");

    const auto fallback = getStatus();
    tuningFallbackType = fallback.deviceType;
    tuningFallbackOutput = fallback.deviceName;
    tuningFallbackRate = fallback.sampleRate;
    tuningFallbackBuffer = fallback.bufferSize;
    tuningResults.clear();
    tuningCandidateIndex = -1;
    buildLatencyCandidates();
    if (tuningCandidates.empty()) return false;
    tuningActive = true;
    if (! startNextLatencyCandidate())
    {
        tuningActive = false;
        return false;
    }
    return true;
}

std::optional<juce::String> AudioDeviceService::pollAutomaticLatencyTuning()
{
    if (! tuningActive) return std::nullopt;
    tuningCandidateMaximumCpu = juce::jmax(tuningCandidateMaximumCpu, manager.getCpuUsage());
    if (juce::Time::getMillisecondCounterHiRes() - tuningCandidateStartedAtMs < latencyCandidateTestMs)
        return std::nullopt;

    const auto status = getStatus();
    const auto xrunsNow = manager.getXRunCount();
    const auto addedXRuns = xrunsNow < 0 ? 0 : juce::jmax(0, xrunsNow - tuningCandidateStartXRuns);
    // 留出至少约 35% 实时音频余量，避免用户开始播放伴奏后才暴露丢音。
    const auto stable = status.ready && addedXRuns == 0 && tuningCandidateMaximumCpu < 0.65;
    const auto score = status.estimatedBufferLatencyMs
        + tuningCandidateMaximumCpu * 10.0
        + static_cast<double>(tuningCandidates[static_cast<size_t>(tuningCandidateIndex)].priority) * 0.15
        + (stable ? 0.0 : 1000.0);
    tuningResults.push_back({ tuningCandidates[static_cast<size_t>(tuningCandidateIndex)], status,
                              tuningCandidateMaximumCpu, addedXRuns, stable, score });
    if (startNextLatencyCandidate()) return std::nullopt;
    return finishAutomaticLatencyTuning();
}

juce::String AudioDeviceService::finishAutomaticLatencyTuning()
{
    const LatencyResult* best = nullptr;
    for (const auto& result : tuningResults)
        if (result.status.ready && result.stable && (best == nullptr || result.score < best->score)) best = &result;

    const LatencyResult* safestTested = nullptr;
    for (const auto& result : tuningResults)
        if (result.status.ready && (safestTested == nullptr
            || result.addedXRuns < safestTested->addedXRuns
            || (result.addedXRuns == safestTested->addedXRuns
                && result.status.bufferSize > safestTested->status.bufferSize)))
            safestTested = &result;

    bool restoredFallback = false;
    if (best != nullptr)
        (void) applyLatencyCandidate({ best->candidate.typeName, best->candidate.outputName,
                                      best->status.bufferSize, best->candidate.priority });
    else if (safestTested != nullptr)
        restoredFallback = applyLatencyCandidate({ safestTested->candidate.typeName,
                                                   safestTested->candidate.outputName,
                                                   safestTested->status.bufferSize,
                                                   safestTested->candidate.priority });
    else if (tuningFallbackType.isNotEmpty())
    {
        restoredFallback = applyLatencyCandidate({ tuningFallbackType, tuningFallbackOutput,
                                                   tuningFallbackBuffer, 9 });
        if (restoredFallback && tuningFallbackRate > 0.0)
        {
            auto setup = manager.getAudioDeviceSetup();
            setup.sampleRate = tuningFallbackRate;
            (void) manager.setAudioDeviceSetup(setup, true);
        }
    }

    tuningActive = false;
    tuningCandidates.clear();
    tuningCandidateIndex = -1;
    const auto applied = getStatus();
    observedXRunCount = juce::jmax(0, manager.getXRunCount());
    unstablePolls = 0;
    if (best == nullptr && ! restoredFallback)
        return juce::String::fromUTF8("未找到可用的声音输出，请检查耳机或音响");
    if (auto* settings = properties.getUserSettings())
    {
        settings->setValue("audioSetupMode", "automatic");
        settings->setValue("automaticAudioDevice", currentDeviceSignature());
        settings->setValue("automaticLatencyTunedDevice", currentDeviceSignature());
        settings->setValue("audioSetupRevision", currentAudioSetupRevision);
    }
    saveSettings();
    if (best == nullptr)
        return juce::String::fromUTF8("已选择实测中最稳定的共享方案；如仍丢音，可在声音设置中手动调整");
    return juce::String::fromUTF8("声音已自动优化，可以开始吹奏（预计延迟 ")
         + juce::String(applied.estimatedBufferLatencyMs, 1) + " ms）";
}

void AudioDeviceService::saveSettings()
{
    if (auto state = manager.createStateXml())
        if (auto* settings = properties.getUserSettings())
        {
            settings->setValue("audioDevice", state->toString());
            settings->saveIfNeeded();
        }
}

juce::AudioIODeviceType* AudioDeviceService::findType(const juce::String& typeName)
{
    for (auto* type : manager.getAvailableDeviceTypes())
        if (type->getTypeName() == typeName)
            return type;
    return nullptr;
}
}
