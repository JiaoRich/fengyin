#include "AudioDeviceService.h"
#include <algorithm>

namespace
{
constexpr int currentAudioSetupRevision = 5;
constexpr double latencyCandidateTestMs = 1800.0;

int chooseLiveBufferSize(juce::Array<int> sizes, int preferred)
{
    if (sizes.isEmpty())
        return preferred;

    std::sort(sizes.begin(), sizes.end());
    if (sizes.contains(preferred))
        return preferred;

    for (const auto size : sizes)
        if (size > preferred)
            return size;

    return sizes.getLast();
}

int recommendedInitialBuffer(const juce::String& deviceType)
{
    const auto cpuCount = juce::SystemStats::getNumCpus();
    const auto memoryMb = juce::SystemStats::getMemorySizeInMegabytes();
    const auto modestComputer = cpuCount <= 4 || (memoryMb > 0 && memoryMb < 8192);
    if (deviceType.containsIgnoreCase("ASIO"))
        return modestComputer ? 128 : 64;
    return modestComputer ? 256 : 128;
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
        if (! type->getTypeName().containsIgnoreCase("Exclusive"))
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
    if (typeName.containsIgnoreCase("Exclusive"))
        return juce::String::fromUTF8("风吟已停用 Windows 独占模式，请选择低延迟共享模式或 ASIO");
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
    // 0.7.4 会把普通 Windows Audio 的系统固定缓冲（常见为 512）
    // 误当成已优化的手动设置。升级后只迁移这一种明显的旧配置，
    // ASIO、独占模式和已经较低的手动缓冲仍保留。
    const auto legacyFixedBuffer = revision < currentAudioSetupRevision
        && current.deviceType.equalsIgnoreCase("Windows Audio")
        && current.bufferSize >= 512;
    const auto legacyExclusiveMode = revision < currentAudioSetupRevision
        && current.deviceType.containsIgnoreCase("Exclusive");
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
    // 已经成功打开厂家 ASIO 时优先保留；不要为了“自动”而切回系统驱动。
    if (auto* current = manager.getCurrentAudioDevice())
        if (current->isOpen() && current->getTypeName().containsIgnoreCase("ASIO"))
            return manager.getCurrentAudioDeviceType();

    // 只有唯一且明确属于硬件厂家的 ASIO 输出时才自动选择。
    // ASIO4ALL/Generic 等包装驱动往往需要用户手工路由，不适合作为“插上就吹”的默认值。
    for (const auto& typeName : types)
    {
        if (! typeName.containsIgnoreCase("ASIO")) continue;
        if (auto* type = findType(typeName))
        {
            type->scanForDevices();
            const auto outputs = type->getDeviceNames(false);
            if (outputs.size() == 1
                && ! outputs[0].containsIgnoreCase("ASIO4ALL")
                && ! outputs[0].containsIgnoreCase("Generic"))
                return typeName;
        }
    }
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

    auto* device = manager.getCurrentAudioDevice();
    auto setup = manager.getAudioDeviceSetup();
    setup.outputDeviceName = outputs[defaultIndex];
    setup.inputDeviceName.clear();
    setup.useDefaultInputChannels = false;
    setup.useDefaultOutputChannels = true;

    if (device != nullptr)
    {
        const auto rates = device->getAvailableSampleRates();
        if (rates.contains(48000.0)) setup.sampleRate = 48000.0;

        setup.bufferSize = chooseLiveBufferSize(device->getAvailableBufferSizes(),
                                                recommendedInitialBuffer(typeName));
    }
    else
    {
        setup.sampleRate = 48000.0;
        setup.bufferSize = 128;
    }

    lastError = manager.setAudioDeviceSetup(setup, true);
    if (lastError.isNotEmpty() && setup.bufferSize < 256)
    {
        setup.bufferSize = 256;
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
    if (tuningActive)
        return false;
    auto* current = manager.getCurrentAudioDevice();
    if (current == nullptr || current->getTypeName().containsIgnoreCase("ASIO"))
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
        return false;
    saveSettings();
    return true;
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
    // “自动优化”必须真正离开系统固定大缓冲的普通共享模式。
    // 已在使用厂家 ASIO 时不强制替换。
    if (! device->getTypeName().containsIgnoreCase("ASIO"))
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

    const auto rates = device->getAvailableSampleRates();
    if (rates.contains(48000.0))
        setup.sampleRate = 48000.0;

    const auto preferred = recommendedInitialBuffer(device->getTypeName());
    const auto chosen = chooseLiveBufferSize(device->getAvailableBufferSizes(), preferred);
    setup.bufferSize = chosen;

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

bool AudioDeviceService::stabiliseAfterXRuns()
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
    if (unstablePolls < 2) return false;

    auto setup = manager.getAudioDeviceSetup();
    auto buffers = device->getAvailableBufferSizes();
    std::sort(buffers.begin(), buffers.end());
    int next = 0;
    for (const auto candidate : buffers)
        if (candidate > setup.bufferSize && candidate <= 256) { next = candidate; break; }
    if (next == 0 && buffers.isEmpty() && setup.bufferSize < 256)
        next = setup.bufferSize < 128 ? 128 : 256;
    if (next == 0) return false;
    setup.bufferSize = next;
    setup.inputDeviceName.clear();
    setup.useDefaultInputChannels = false;
    lastError = manager.setAudioDeviceSetup(setup, true);
    unstablePolls = 0;
    observedXRunCount = manager.getXRunCount();
    if (lastError.isNotEmpty()) return false;
    saveSettings();
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
            if (existing.typeName == typeName && existing.outputName == outputName) return;
        tuningCandidates.push_back({ typeName, outputName, preferredBuffer, priority });
    };

    const auto types = getAvailableDeviceTypes();
   #if JUCE_WINDOWS
    // 硬件厂家 ASIO 只在唯一且无需人工路由时自动尝试。
    for (const auto& typeName : types)
    {
        if (! typeName.containsIgnoreCase("ASIO")) continue;
        const auto outputs = getAvailableOutputDevices(typeName);
        if (outputs.size() == 1 && ! outputs[0].containsIgnoreCase("ASIO4ALL")
            && ! outputs[0].containsIgnoreCase("Generic"))
            add(typeName, outputs[0], recommendedInitialBuffer(typeName), 0);
    }
    for (const auto& typeName : types)
    {
        if (! (typeName.containsIgnoreCase("Low Latency Mode")
               || typeName.containsIgnoreCase(juce::String::fromUTF8("低延迟")))) continue;
        if (auto* type = findType(typeName))
        {
            type->scanForDevices();
            const auto outputs = type->getDeviceNames(false);
            const auto index = type->getDefaultDeviceIndex(false);
            if (juce::isPositiveAndBelow(index, outputs.size()))
                add(typeName, outputs[index], recommendedInitialBuffer(typeName), 1);
        }
    }
    // 如果用户已自行安装 ASIO4ALL，可以试跑；风吟不捆绑或静默安装第三方驱动。
    for (const auto& typeName : types)
    {
        if (! typeName.containsIgnoreCase("ASIO")) continue;
        const auto outputs = getAvailableOutputDevices(typeName);
        if (outputs.size() == 1 && outputs[0].containsIgnoreCase("ASIO4ALL"))
            add(typeName, outputs[0], 128, 2);
    }
   #endif
    // 系统共享模式永远作为最后的兼容性候选。
    for (const auto& typeName : types)
    {
        if (typeName.containsIgnoreCase("DirectSound") || typeName.containsIgnoreCase("ASIO")
            || typeName.containsIgnoreCase("Exclusive") || typeName.containsIgnoreCase("Low Latency Mode"))
            continue;
        if (auto* type = findType(typeName))
        {
            type->scanForDevices();
            const auto outputs = type->getDeviceNames(false);
            const auto index = type->getDefaultDeviceIndex(false);
            if (juce::isPositiveAndBelow(index, outputs.size())) add(typeName, outputs[index], 256, 3);
        }
    }
    const auto current = getStatus();
    add(current.deviceType, current.deviceName,
        current.bufferSize > 0 ? juce::jmin(256, current.bufferSize) : 128, 5);
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
        const auto rates = device->getAvailableSampleRates();
        if (rates.contains(48000.0)) setup.sampleRate = 48000.0;
        else if (! rates.isEmpty()) setup.sampleRate = rates[0];
        setup.bufferSize = chooseLiveBufferSize(device->getAvailableBufferSizes(), candidate.preferredBuffer);
    }
    else
    {
        setup.sampleRate = 48000.0;
        setup.bufferSize = candidate.preferredBuffer;
    }
    lastError = manager.setAudioDeviceSetup(setup, true);
    if (lastError.isNotEmpty() && setup.bufferSize < 256)
    {
        setup.bufferSize = 256;
        lastError = manager.setAudioDeviceSetup(setup, true);
    }
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
        if (result.status.ready && (best == nullptr || result.score < best->score)) best = &result;

    bool restoredFallback = false;
    if (best != nullptr)
        (void) applyLatencyCandidate({ best->candidate.typeName, best->candidate.outputName,
                                      best->status.bufferSize, best->candidate.priority });
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
    if (best == nullptr)
        return restoredFallback ? juce::String::fromUTF8("已保留上一个可用的声音方案")
                                : juce::String::fromUTF8("未找到可用的声音输出，请检查耳机或音响");

    if (auto* settings = properties.getUserSettings())
    {
        settings->setValue("audioSetupMode", "automatic");
        settings->setValue("automaticAudioDevice", currentDeviceSignature());
        settings->setValue("automaticLatencyTunedDevice", currentDeviceSignature());
        settings->setValue("audioSetupRevision", currentAudioSetupRevision);
    }
    saveSettings();
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
