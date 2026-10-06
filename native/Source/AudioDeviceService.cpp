#include "AudioDeviceService.h"
#include "AudioHardwareIdentity.h"
#include "FengYinEngineAudioIODevice.h"
#include "../../audio-engine/common/VirtualEndpointChoice.h"
#include <algorithm>

namespace
{
constexpr int currentAudioSetupRevision = 9;
constexpr int currentTuningRevision = 8;
constexpr double latencyCandidateTestMs = 3500.0;
const auto engineModeName = juce::String::fromUTF8("风吟低延迟（ASIO4ALL）");

bool isWindowsSharedType(const juce::String& typeName)
{
   #if JUCE_WINDOWS
    return typeName == "Windows Audio"
        || typeName.containsIgnoreCase("Low Latency Mode")
        || typeName.containsIgnoreCase("RAW Test Mode")
        || typeName.containsIgnoreCase(juce::String::fromUTF8("低延迟"));
   #else
    return ! typeName.containsIgnoreCase("Exclusive") && ! typeName.containsIgnoreCase("ASIO");
   #endif
}

bool isAsioType(const juce::String& typeName)
{
    return typeName.equalsIgnoreCase("ASIO");
}

bool isSupportedSharedAsioDevice(const juce::String& deviceName)
{
    return deviceName.containsIgnoreCase("KoordASIO");
}

#if JUCE_WINDOWS
bool savedStateIsSupportedSharedAsio(const juce::XmlElement& state)
{
    if (! isAsioType(state.getStringAttribute("deviceType"))) return false;
    const auto output = state.getStringAttribute("audioOutputDeviceName",
                        state.getStringAttribute("audioDeviceName"));
    return isSupportedSharedAsioDevice(output);
}
#endif

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

AudioDeviceService::~AudioDeviceService()
{
    stopTimer();
    manager.closeAudioDevice();
    engineProcess.stop();
}

juce::String AudioDeviceService::initialise()
{
   #if JUCE_WINDOWS
    // A public build enters the isolated engine only when its signed virtual
    // speaker is actually active. Developer packages can opt in explicitly;
    // machines without the complete driver fall through immediately to the
    // established Windows shared path, with no startup delay or system change.
    auto* audioSettings = properties.getUserSettings();
    const auto engineEnabled = audioSettings == nullptr
        || audioSettings->getBoolValue("audioEngineEnabled", true);
    if (engineEnabled && (audioengine::AudioEngineProcessController::isAvailable()
        || juce::SystemStats::getEnvironmentVariable("FENGYIN_AUDIO_ENGINE_TEST", {}) == "1"))
    {
        const auto requestedFrames = audioSettings != nullptr
            ? audioSettings->getIntValue("audioEngineBuffer", 128) : 128;
        lastError = startIsolatedAudioEngine(requestedFrames);
        if (lastError.isEmpty()) return {};
    }
   #endif
    std::unique_ptr<juce::XmlElement> saved;
    if (auto* settings = properties.getUserSettings())
        saved = juce::parseXML(settings->getValue("audioDevice"));
   #if JUCE_WINDOWS
    // 旧版 SAR/ASIO4ALL/Exclusive 设置全部废弃；只允许用户主动保存的
    // KoordASIO 共享输出在下次启动恢复。
    const auto restoringSharedAsio = saved != nullptr && savedStateIsSupportedSharedAsio(*saved);
    if (saved != nullptr && ! isWindowsSharedType(saved->getStringAttribute("deviceType"))
        && ! restoringSharedAsio)
        saved.reset();
   #endif
    lastError = manager.initialise(0, 2, saved.get(), true);
   #if JUCE_WINDOWS
    if (restoringSharedAsio && (lastError.isNotEmpty() || manager.getCurrentAudioDevice() == nullptr))
    {
        manager.closeAudioDevice();
        lastError = manager.initialise(0, 2, nullptr, true);
    }
   #endif
    return lastError;
}

juce::String AudioDeviceService::startIsolatedAudioEngine(int requestedFrames)
{
#if JUCE_WINDOWS
    // selectDeviceType closes the client device before entering here. A live
    // engine does not imply that the instrument producer is still attached.
    if (requestedFrames != 128 && requestedFrames != 256 && requestedFrames != 512)
        requestedFrames = 128;
    juce::String engineError;
    if (! engineProcess.isRunning()
        && ! engineProcess.start(static_cast<std::uint32_t>(requestedFrames), {}, true, engineError))
        return engineError;
    if (! engineDeviceTypeAdded)
    {
        manager.addAudioDeviceType(std::make_unique<audioengine::FengYinEngineAudioIODeviceType>());
        engineDeviceTypeAdded = true;
    }
    juce::XmlElement engineState("DEVICESETUP");
    engineState.setAttribute("deviceType", "FengYin Audio Engine");
    engineState.setAttribute("audioOutputDeviceName", "FengYin Low Latency Output");
    engineState.setAttribute("audioInputDeviceName", juce::String());
    engineState.setAttribute("audioDeviceRate", static_cast<double>(audioengine::engineSampleRate));
    engineState.setAttribute("audioDeviceBufferSize",
                             static_cast<int>(engineProcess.actualBufferFrames()));
    engineState.setAttribute("audioDeviceInChans", juce::String());
    engineState.setAttribute("audioDeviceOutChans", "11");
    auto result = manager.initialise(0, 2, &engineState, false);
    if (result.isNotEmpty())
    {
        manager.closeAudioDevice();
        engineProcess.stop();
        return result;
    }
    startTimer(250);
    return {};
#else
    return juce::String::fromUTF8("风吟低延迟音频引擎仅支持 Windows");
#endif
}

void AudioDeviceService::timerCallback()
{
   #if JUCE_WINDOWS
    if (engineProcess.isRunning()) return;
    stopTimer();
    manager.closeAudioDevice();
    engineProcess.stop();
    const auto fallbackError = manager.initialise(0, 2, nullptr, true);
    lastError = fallbackError.isEmpty()
        ? juce::String::fromUTF8("低延迟声音引擎已自动恢复为 Windows 共享输出")
        : juce::String::fromUTF8("低延迟声音引擎和 Windows 共享输出均未能恢复：") + fallbackError;
   #endif
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
        if (engineProcess.isRunning())
        {
            status.deviceType = engineModeName;
            status.deviceName = juce::String::fromUTF8("ASIO4ALL（输出由驱动控制面板选择）");
        }
        status.sampleRate = device->getCurrentSampleRate();
        status.bufferSize = device->getCurrentBufferSizeSamples();
        if (status.sampleRate > 0.0)
        {
            // JUCE/WASAPI 报告的 output latency 通常已包含当前输出周期，
            // 不能再把 bufferSize 相加，否则 512 会被错显示为约 22 ms。
            const auto reported = device->getOutputLatencyInSamples();
            status.estimatedBufferLatencyMs = juce::jmax(status.bufferSize, reported) * 1000.0 / status.sampleRate;
        }
        status.cpuUsage = manager.getCpuUsage();
        status.xRunCount = manager.getXRunCount();
    }
    return status;
}

juce::StringArray AudioDeviceService::getAvailableDeviceTypes()
{
    juce::StringArray names;
   #if JUCE_WINDOWS
    if (audioengine::AudioEngineProcessController::isAvailable()) names.add(engineModeName);
   #endif
    for (auto* type : manager.getAvailableDeviceTypes())
    {
        if (isWindowsSharedType(type->getTypeName()))
            names.add(type->getTypeName());
        else if (isAsioType(type->getTypeName()))
        {
            type->scanForDevices();
            for (const auto& device : type->getDeviceNames(false))
                if (isSupportedSharedAsioDevice(device))
                {
                    names.add(type->getTypeName());
                    break;
                }
        }
    }
    return names;
}

juce::StringArray AudioDeviceService::getAvailableOutputDevices(const juce::String& typeName)
{
    if (typeName == engineModeName)
        return { juce::String::fromUTF8("ASIO4ALL（输出由驱动控制面板选择）") };
    if (auto* type = findType(typeName))
    {
        type->scanForDevices();
        auto devices = type->getDeviceNames(false);
        if (isAsioType(typeName))
            for (int index = devices.size() - 1; index >= 0; --index)
                if (! isSupportedSharedAsioDevice(devices[index])) devices.remove(index);
        return devices;
    }
    return {};
}

bool AudioDeviceService::isSharedAsioModeAvailable()
{
    return getAvailableDeviceTypes().contains("ASIO")
        && ! getAvailableOutputDevices("ASIO").isEmpty();
}

bool AudioDeviceService::isSharedAsioModeActive()
{
    const auto status = getStatus();
    return isAsioType(status.deviceType) && isSupportedSharedAsioDevice(status.deviceName);
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

juce::String AudioDeviceService::configureAsio4All()
{
   #if JUCE_WINDOWS
    stopTimer();
    tuningActive = false;
    manager.closeAudioDevice();
    engineProcess.stop();
    if (auto* settings = properties.getUserSettings())
        settings->setValue("audioEngineEnabled", false);
    const auto executable = juce::File::getSpecialLocation(juce::File::currentExecutableFile)
        .getSiblingFile("FengYinAudioEngine.exe");
    if (! executable.startAsProcess("--configure-asio"))
        return juce::String::fromUTF8("无法打开 ASIO4ALL 配置程序");
    return juce::String::fromUTF8("已暂停风吟音频。请在 ASIO4ALL 中仅启用实际扬声器/耳机输出，关闭输入和风吟虚拟扬声器。关闭面板后重新选择风吟低延迟模式。");
   #else
    return juce::String::fromUTF8("ASIO4ALL 仅用于 Windows");
   #endif
}

juce::String AudioDeviceService::selectDeviceType(const juce::String& typeName)
{
   #if JUCE_WINDOWS
    if (typeName == engineModeName)
    {
        if (! audioengine::AudioEngineProcessController::isAvailable())
            return juce::String::fromUTF8(fengyin::audioengine::vbCableTrial()
                ? "VB-CABLE 尚未安装或 CABLE Input 未启用，请安装基础版 VB-CABLE 后重启电脑"
                : "风吟共享扬声器尚未安装或未启用");
        manager.closeAudioDevice();
        auto* settings = properties.getUserSettings();
        const auto requestedFrames = settings != nullptr
            ? settings->getIntValue("audioEngineBuffer", 128) : 128;
        lastError = startIsolatedAudioEngine(requestedFrames);
        if (lastError.isEmpty())
        {
            if (auto* settings = properties.getUserSettings())
                settings->setValue("audioEngineEnabled", true);
            saveSettings();
        }
        else
        {
            (void) manager.initialise(0, 2, nullptr, true);
        }
        return lastError;
    }
   #endif
    if (! isWindowsSharedType(typeName) && ! (isAsioType(typeName) && isSharedAsioModeAvailable()))
        return juce::String::fromUTF8("仅支持 Windows 共享输出或 KoordASIO 共享低延迟驱动");

   #if JUCE_WINDOWS
    // This is the explicit advanced-user escape hatch. Stop the isolated
    // engine first so it restores the physical Windows endpoint and releases
    // exclusive ownership before JUCE opens the manually selected backend.
    if (engineProcess.isRunning())
    {
        manager.closeAudioDevice();
        engineProcess.stop();
        stopTimer();
    }
   #endif

    if (isAsioType(typeName))
    {
        const auto devices = getAvailableOutputDevices(typeName);
        if (devices.isEmpty())
            return juce::String::fromUTF8("未检测到 KoordASIO，请先安装后重新打开风吟");

        // AudioDeviceManager::setCurrentAudioDeviceType("ASIO") 会先打开注册表中的
        // 默认 ASIO 驱动；当机器还装有 ASIO4ALL 或残留 SAR 时可能误开并卡住。
        // 用精确 XML 直接指定 KoordASIO，保证其他 ASIO 驱动永远不会被实例化。
        auto previousState = manager.createStateXml();
        juce::XmlElement koordState("DEVICESETUP");
        koordState.setAttribute("deviceType", "ASIO");
        koordState.setAttribute("audioOutputDeviceName", devices[0]);
        koordState.setAttribute("audioInputDeviceName", juce::String());
        koordState.setAttribute("audioDeviceRate", 48000.0);
        koordState.setAttribute("audioDeviceBufferSize", 128);
        koordState.setAttribute("audioDeviceInChans", juce::String());
        koordState.setAttribute("audioDeviceOutChans", "11");
        manager.closeAudioDevice();
        lastError = manager.initialise(0, 2, &koordState, false);
        if (lastError.isNotEmpty() && previousState != nullptr)
            (void) manager.initialise(0, 2, previousState.get(), true);
        if (lastError.isEmpty())
        {
            if (auto* settings = properties.getUserSettings())
                settings->setValue("audioEngineEnabled", false);
            saveSettings();
        }
        return lastError;
    }

    manager.setCurrentAudioDeviceType(typeName, true);
    lastError = manager.getCurrentAudioDeviceType() == typeName
        ? juce::String()
        : juce::String::fromUTF8("无法启用所选声音驱动");
    if (lastError.isEmpty())
    {
        if (auto* settings = properties.getUserSettings())
            settings->setValue("audioEngineEnabled", false);
        saveSettings();
    }
    return lastError;
}

juce::String AudioDeviceService::applyOutputSetup(const juce::String& outputName,
                                                   double sampleRate,
                                                   int bufferSize)
{
    tuningActive = false;
    tuningCandidates.clear();
    tuningResults.clear();
   #if JUCE_WINDOWS
    if (engineProcess.isRunning())
    {
        if (bufferSize != 128 && bufferSize != 256 && bufferSize != 512)
            return juce::String::fromUTF8("风吟低延迟模式仅支持 128、256 或 512 采样");
        if (std::abs(sampleRate - static_cast<double>(audioengine::engineSampleRate)) > 0.5)
            return juce::String::fromUTF8("风吟低延迟模式固定使用 48000 Hz");
        manager.closeAudioDevice();
        engineProcess.stop();
        lastError = startIsolatedAudioEngine(bufferSize);
        if (lastError.isEmpty())
        {
            if (auto* settings = properties.getUserSettings())
            {
                settings->setValue("audioEngineEnabled", true);
                settings->setValue("audioEngineBuffer", bufferSize);
                settings->setValue("audioSetupMode", "manual");
            }
            saveSettings();
        }
        else
        {
            (void) manager.initialise(0, 2, nullptr, true);
        }
        return lastError;
    }
   #endif
    if (isAsioType(manager.getCurrentAudioDeviceType()) && ! isSupportedSharedAsioDevice(outputName))
        return juce::String::fromUTF8("共享低延迟模式只能选择 KoordASIO");
    const auto previousSetup = manager.getAudioDeviceSetup();
    const auto sharedAsioRequest = isAsioType(manager.getCurrentAudioDeviceType())
        && isSupportedSharedAsioDevice(outputName);
    auto setup = previousSetup;
    setup.outputDeviceName = outputName;
    setup.inputDeviceName.clear();
    setup.sampleRate = sampleRate;
    setup.bufferSize = bufferSize;
    setup.useDefaultInputChannels = false;
    setup.inputChannels.clear();
    setup.useDefaultOutputChannels = true;
    lastError = manager.setAudioDeviceSetup(setup, true);
    if (lastError.isNotEmpty() && sharedAsioRequest)
    {
        const auto requestedError = lastError;
        manager.closeAudioDevice();
        const auto restoreError = manager.setAudioDeviceSetup(previousSetup, true);
        if (restoreError.isEmpty())
        {
            lastError.clear();
            return juce::String::fromUTF8("新缓冲区 ") + juce::String(bufferSize)
                + juce::String::fromUTF8(" 无法启动，已保留 KoordASIO 并恢复原缓冲区。驱动返回：")
                + requestedError;
        }
        lastError = requestedError + juce::String::fromUTF8("；恢复原 KoordASIO 设置也失败：") + restoreError;
        return lastError;
    }
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
    if (engineProcess.isRunning())
        return juce::String::fromUTF8("风吟低延迟音频引擎已启动");
    auto* settings = properties.getUserSettings();
    const auto current = getStatus();
    const auto revision = settings != nullptr ? settings->getIntValue("audioSetupRevision", 0) : 0;
    // 升级后迁移旧版误判的固定大缓冲，并强制废弃任何已保存的
    // ASIO/独占路径；只保留用户亲自选择的 Windows 共享参数。
    const auto legacyFixedBuffer = revision < currentAudioSetupRevision
        && current.deviceType.equalsIgnoreCase("Windows Audio")
        && current.bufferSize >= 512;
    const auto legacyExclusiveMode = revision < currentAudioSetupRevision
        && ! isWindowsSharedType(current.deviceType) && ! isSharedAsioModeActive();
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

    // 恢复 Windows 当前默认端点；低延迟验证另按物理声卡与驱动版本记录。
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
    if (tuningActive)
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
        // Endpoint changes preserve the hardware/driver validation. No silent probe.
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
    if (tuningActive) return false;
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
    if (engineProcess.isRunning()) return false;
    auto* settings = properties.getUserSettings();
    if (settings == nullptr || settings->getValue("audioSetupMode") == "manual")
        return false;
    const auto identity = tuningIdentity();
    if (identity.isEmpty() || identity == attemptedTuningIdentity) return false;
    return settings->getIntValue("audioTuningRevision", 0) < currentTuningRevision
        || settings->getValue("audioTunedHardware") != identity;
}

juce::String AudioDeviceService::tuningIdentity()
{
    const auto now = juce::Time::getMillisecondCounterHiRes();
    if (now - hardwareIdentityCheckedAt > 10000.0)
    {
        cachedHardwareIdentity = audioHardwareIdentity();
        hardwareIdentityCheckedAt = now;
    }
    return cachedHardwareIdentity.isEmpty() ? juce::String()
        : manager.getCurrentAudioDeviceType() + "|" + cachedHardwareIdentity;
}

double AudioDeviceService::getTuningProgress() const noexcept
{
    if (! tuningActive || tuningCandidates.empty()) return 0.0;
    const auto fraction = juce::jlimit(0.0, 1.0,
        (juce::Time::getMillisecondCounterHiRes() - tuningCandidateStartedAtMs) / latencyCandidateTestMs);
    return juce::jlimit(0.0, 0.99, (tuningCandidateIndex + fraction) / static_cast<double>(tuningCandidates.size()));
}

void AudioDeviceService::cancelAutomaticLatencyTuning()
{
    if (! tuningActive) return;
    tuningActive = false;
    (void) applyLatencyCandidate({ tuningFallbackType, tuningFallbackOutput, tuningFallbackBuffer, 0 });
    tuningCandidates.clear();
    tuningCandidateIndex = -1;
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
    return lastError.isEmpty() && status.ready && status.bufferSize == candidate.preferredBuffer;
}

bool AudioDeviceService::startNextLatencyCandidate()
{
    while (++tuningCandidateIndex < static_cast<int>(tuningCandidates.size()))
    {
        if (! applyLatencyCandidate(tuningCandidates[static_cast<size_t>(tuningCandidateIndex)]))
            continue;
        tuningCandidateStartedAtMs = juce::Time::getMillisecondCounterHiRes();
        startCallbacks = probeCallbacks;
        startOverruns = probeOverruns;
        startSignals = probeSignals;
        tuningCandidateStartXRuns = juce::jmax(0, manager.getXRunCount());
        tuningCandidateMaximumCpu = manager.getCpuUsage();
        return true;
    }
    return false;
}

bool AudioDeviceService::beginAutomaticLatencyTuning(bool force)
{
    if (engineProcess.isRunning()) return false;
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
    if (force)
    {
        const auto preferred = preferredLiveDeviceType();
        if (preferred.isNotEmpty() && preferred != fallback.deviceType)
            if (configureAutomaticType(preferred).isNotEmpty()) return false;
    }
    attemptedTuningIdentity = tuningIdentity();
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
    const auto stable = status.ready && addedXRuns == 0 && tuningCandidateMaximumCpu < 0.65
        && probeCallbacks > startCallbacks + 20 && probeSignals > startSignals + 5
        && probeOverruns == startOverruns;
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

    bool restoredFallback = false;
    if (best != nullptr)
    {
        if (! applyLatencyCandidate({ best->candidate.typeName, best->candidate.outputName,
                                      best->status.bufferSize, best->candidate.priority })) best = nullptr;
        else restoredFallback = true;
    }
    if (best == nullptr && tuningFallbackType.isNotEmpty())
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
    if (best != nullptr)
      if (auto* settings = properties.getUserSettings())
    {
        settings->setValue("audioSetupMode", "automatic");
        settings->setValue("automaticAudioDevice", currentDeviceSignature());
        settings->setValue("automaticLatencyTunedDevice", currentDeviceSignature());
        settings->setValue("audioTunedHardware", tuningIdentity());
        settings->setValue("audioTuningRevision", currentTuningRevision);
        settings->setValue("audioSetupRevision", currentAudioSetupRevision);
    }
    saveSettings();
    if (best == nullptr)
        return juce::String::fromUTF8("未通过稳定性验证，未保存为已优化；请调整缓冲后重试");
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
