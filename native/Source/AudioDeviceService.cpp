#include "AudioDeviceService.h"
#include "AudioHardwareIdentity.h"
#include "FengYinEngineAudioIODevice.h"
#include "../../audio-engine/common/VirtualEndpointChoice.h"
#include <algorithm>
#if JUCE_WINDOWS
#include <mmdeviceapi.h>
#include <functiondiscoverykeys_devpkey.h>
#include <wrl/client.h>
#endif

namespace
{
constexpr int currentAudioSetupRevision = 10;
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
bool isVirtualAudioOutput(const juce::String& name)
{
    return name.containsIgnoreCase("cable") || name.containsIgnoreCase("vb-audio")
        || name.containsIgnoreCase("fengyin") || name.containsIgnoreCase("voicemeeter")
        || name.contains(juce::String::fromUTF8("风吟共享"));
}

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
    if (asioPanelOpen) asioPanelProcess.kill();
    manager.closeAudioDevice();
    engineProcess.stop();
}

juce::String AudioDeviceService::initialise()
{
    juce::String startupError;
   #if JUCE_WINDOWS
    // Default to physical Windows shared output. ASIO bridge is opt-in.
    auto* audioSettings = properties.getUserSettings();
    if (audioSettings != nullptr && audioSettings->getIntValue("productionAudioRouteRevision", 0) < 3)
    {
        audioSettings->setValue("productionAudioRouteRevision", 3);
        audioSettings->setValue("audioEngineEnabled", false);
        audioSettings->removeValue("audioDevice");
        if (!audioSettings->containsKey("audioEngineBuffer"))
            audioSettings->setValue("audioEngineBuffer", 128);
    }
    const auto engineEnabled = audioSettings != nullptr
        && audioSettings->getBoolValue("audioEngineEnabled", false);
    if (engineEnabled && (audioengine::AudioEngineProcessController::isAvailable()
        || juce::SystemStats::getEnvironmentVariable("FENGYIN_AUDIO_ENGINE_TEST", {}) == "1"))
    {
        const auto requestedFrames = audioSettings != nullptr
            ? audioSettings->getIntValue("audioEngineBuffer", 128) : 128;
        lastError = startIsolatedAudioEngine(requestedFrames);
        if (lastError.isEmpty()) return {};
        startupError = lastError;
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
    if (saved != nullptr && isVirtualAudioOutput(saved->getStringAttribute("audioOutputDeviceName")))
        saved.reset();
    // Never ask JUCE to choose the system default: it may still be CABLE
    // while the crashed engine's route-restoration watchdog is running.
    lastError = saved != nullptr ? manager.initialise(0, 2, saved.get(), false)
                                : restorePhysicalSharedOutput();
    if (lastError.isNotEmpty() || manager.getCurrentAudioDevice() == nullptr)
        lastError = restorePhysicalSharedOutput();
    if (startupError.isNotEmpty())
        lastError = startupError + (lastError.isEmpty()
            ? juce::String::fromUTF8("；已临时使用实际设备的共享输出，低延迟模式尚未恢复")
            : juce::String::fromUTF8("；共享输出恢复失败：") + lastError);
   #else
    lastError = manager.initialise(0, 2, saved.get(), true);
   #endif
    return lastError;
}

juce::String AudioDeviceService::restorePhysicalSharedOutput()
{
    manager.closeAudioDevice();
    engineProcess.stop();
   #if JUCE_WINDOWS
    juce::String failure = juce::String::fromUTF8("未找到可用的实际扬声器或耳机");
    for (const auto& typeName : { juce::String("Windows Audio (Low Latency Mode)"), juce::String("Windows Audio") })
    {
        auto* type = findType(typeName);
        if (type == nullptr) continue;
        type->scanForDevices();
        auto names = type->getDeviceNames(false);
        const auto defaultIndex = type->getDefaultDeviceIndex(false);
        if (juce::isPositiveAndBelow(defaultIndex, names.size()))
            names.move(defaultIndex, 0);
        for (const auto& name : names)
        {
            if (isVirtualAudioOutput(name)) continue;
            juce::XmlElement state("DEVICESETUP");
            state.setAttribute("deviceType", typeName);
            state.setAttribute("audioOutputDeviceName", name);
            state.setAttribute("audioInputDeviceName", juce::String());
            state.setAttribute("audioDeviceOutChans", "11");
            failure = manager.initialise(0, 2, &state, false);
            if (failure.isEmpty() && manager.getCurrentAudioDevice() != nullptr)
            {
                auto setup = manager.getAudioDeviceSetup();
                if (manager.getCurrentAudioDevice()->getAvailableBufferSizes().contains(128))
                {
                    setup.bufferSize = 128;
                    const auto bufferError = manager.setAudioDeviceSetup(setup, false);
                    if (bufferError.isNotEmpty())
                        juce::Logger::writeToLog("Shared 128-frame request rejected: " + bufferError);
                }
                if (manager.getCurrentAudioDevice() != nullptr) return {};
            }
            manager.closeAudioDevice();
        }
    }
    return failure.isNotEmpty() ? failure : juce::String::fromUTF8("实际音频设备未启动");
   #else
    return manager.initialise(0, 2, nullptr, true);
   #endif
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
        && ! engineProcess.start(static_cast<std::uint32_t>(requestedFrames),
            properties.getUserSettings()->getValue("physicalEndpoint"), true, engineError))
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
    engineRecoveryStartedAt = 0.0;
    startTimer(250);
    return {};
#else
    return juce::String::fromUTF8("风吟低延迟音频引擎仅支持 Windows");
#endif
}

void AudioDeviceService::timerCallback()
{
   #if JUCE_WINDOWS
    if (asioPanelOpen)
    {
        if (asioPanelProcess.isRunning()) return;
        restoreAfterAsioPanel();
        return;
    }
    if (engineProcess.isRecovering())
    {
        const auto now = juce::Time::getMillisecondCounterHiRes();
        if (engineRecoveryStartedAt == 0.0) engineRecoveryStartedAt = now;
        // A driver can block inside start/stop. The isolated process must not
        // leave the application silently waiting forever after a jack change.
        if (now - engineRecoveryStartedAt < 15000.0) return;
        juce::Logger::writeToLog("Audio recovery exceeded 15s; stopping isolated engine before physical fallback");
    }
    else
    {
        engineRecoveryStartedAt = 0.0;
        if (engineProcess.isRunning()) return;
    }
    stopTimer();
    manager.closeAudioDevice();
    engineProcess.stop();
    const auto fallbackError = restorePhysicalSharedOutput();
    lastError = fallbackError.isEmpty()
        ? juce::String::fromUTF8("低延迟引擎已停止；已临时使用实际设备的共享输出，低延迟模式尚未恢复")
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
            if (engineProcess.isRecovering())
            {
                status.ready = false;
                status.error = juce::String::fromUTF8("正在恢复音频输出，请稍候");
            }
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
    names.add(engineModeName); // Missing prerequisites get an explicit error, never a hidden mode.
   #endif
    for (auto* type : manager.getAvailableDeviceTypes())
    {
        if (isWindowsSharedType(type->getTypeName()) && !type->getTypeName().containsIgnoreCase("RAW Test Mode"))
            names.add(type->getTypeName());
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
       #if JUCE_WINDOWS
        if (isWindowsSharedType(typeName))
            for (int index = devices.size() - 1; index >= 0; --index)
                if (isVirtualAudioOutput(devices[index])) devices.remove(index);
       #endif
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
    if (asioPanelOpen) return juce::String::fromUTF8("ASIO4ALL 设置面板已打开，关闭后将自动恢复声音。");
    beforeAsioPanel = manager.createStateXml();
    resumeIsolatedEngine = engineProcess.isRunning();
    juce::Logger::writeToLog("ASIO panel opening: engineRunning=" + juce::String(resumeIsolatedEngine ? 1 : 0));
    if (auto* settings = properties.getUserSettings())
        resumeEngineBuffer = settings->getIntValue("audioEngineBuffer", 128);
    stopTimer();
    manager.closeAudioDevice();
    engineProcess.stop();
    const auto executable = juce::File::getSpecialLocation(juce::File::currentExecutableFile)
        .getSiblingFile("FengYinAudioEngine.exe");
    if (! asioPanelProcess.start(juce::StringArray { executable.getFullPathName(), "--configure-asio" }, 0))
    {
        restoreAfterAsioPanel();
        return juce::String::fromUTF8("无法打开 ASIO4ALL 配置程序");
    }
    asioPanelOpen = true;
    startTimer(250);
    return juce::String::fromUTF8("已暂停音频，关闭 ASIO4ALL 面板后将自动恢复声音。");
   #else
    return juce::String::fromUTF8("ASIO4ALL 仅用于 Windows");
   #endif
}


void AudioDeviceService::restoreAfterAsioPanel()
{
    juce::Logger::writeToLog("ASIO panel closed: restoring previous audio mode");
    asioPanelOpen = false;
    stopTimer();
    lastError = resumeIsolatedEngine ? startIsolatedAudioEngine(resumeEngineBuffer)
        : manager.initialise(0, 2, beforeAsioPanel.get(), false);
    beforeAsioPanel.reset();
    juce::Logger::writeToLog("ASIO panel restore result: " + (lastError.isEmpty() ? juce::String("success") : lastError));
    if (lastError.isEmpty()) return;
    manager.closeAudioDevice();
    engineProcess.stop();
    const auto recoveryError = restorePhysicalSharedOutput();
    if (recoveryError.isNotEmpty()) lastError += "; " + recoveryError;
}

juce::String AudioDeviceService::selectDeviceType(const juce::String& typeName)
{
    if (asioPanelOpen) return juce::String::fromUTF8("请先关闭 ASIO4ALL 面板，声音将自动恢复。");
    if (auto* settings = properties.getUserSettings())
        settings->setValue("audioModeExplicitChoice", true);
   #if JUCE_WINDOWS
    if (typeName == engineModeName)
    {
        if (! bridgeDependenciesAvailable())
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
            const auto recoveryError = restorePhysicalSharedOutput();
            lastError += recoveryError.isEmpty()
                ? juce::String::fromUTF8("；已切换到实际声卡的 Windows 共享输出")
                : juce::String::fromUTF8("；恢复实际声卡失败：") + recoveryError;
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

bool AudioDeviceService::bridgeDependenciesAvailable()
{
   #if JUCE_WINDOWS
    if (!audioengine::AudioEngineProcessController::isAvailable()) return false;
    HKEY key = nullptr;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\ASIO", 0, KEY_READ | KEY_WOW64_64KEY, &key) != ERROR_SUCCESS) return false;
    bool found = false;
    wchar_t name[256]; DWORD length = 256;
    for (DWORD index=0; RegEnumKeyExW(key,index,name,&length,nullptr,nullptr,nullptr,nullptr)==ERROR_SUCCESS; ++index,length=256)
        if (juce::String(name).containsIgnoreCase("ASIO4ALL")) { found=true; break; }
    RegCloseKey(key); return found;
   #else
    return false;
   #endif
}

juce::String AudioDeviceService::applyOutputSetup(const juce::String& outputName,
                                                   double sampleRate,
                                                   int bufferSize)
{
    if (asioPanelOpen) return juce::String::fromUTF8("请先关闭 ASIO4ALL 面板，声音将自动恢复。");
   #if JUCE_WINDOWS
    if (!engineProcess.isRunning() && isVirtualAudioOutput(outputName))
        return juce::String::fromUTF8("请选择实际扬声器或耳机，不能将虚拟线缆作为最终声音输出。");
   #endif
   #if JUCE_WINDOWS
    if (engineProcess.isRunning())
    {
        if (bufferSize != 128 && bufferSize != 256 && bufferSize != 512)
            return juce::String::fromUTF8("风吟低延迟模式仅支持 128、256 或 512 采样");
        if (std::abs(sampleRate - static_cast<double>(audioengine::engineSampleRate)) > 0.5)
            return juce::String::fromUTF8("风吟低延迟模式固定使用 48000 Hz");
        const auto previousFrames = manager.getCurrentAudioDevice() != nullptr
            ? manager.getCurrentAudioDevice()->getCurrentBufferSizeSamples() : 128;
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
            const auto requestedError = lastError;
            const auto recoveryError = startIsolatedAudioEngine(previousFrames);
            lastError = requestedError + (recoveryError.isEmpty()
                ? juce::String::fromUTF8("；已恢复原缓冲区")
                : juce::String::fromUTF8("；恢复失败：") + recoveryError);
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

    // 恢复 Windows 当前默认端点，保留已保存的声音设置。
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
        return juce::String::fromUTF8("已恢复声音设置");

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
        if (type != engineModeName && (type.containsIgnoreCase("Low Latency Mode")
            || type.containsIgnoreCase(juce::String::fromUTF8("低延迟"))))
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
        setup.bufferSize = device->getAvailableBufferSizes().contains(128) ? 128
            : minimumLegalBuffer(device->getAvailableBufferSizes(), device->getCurrentBufferSizeSamples());
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
        // A new endpoint may have a different mix rate. Retry its mix rate,
        // but retain the requested period: buffer=0 silently enlarges 128 to 480.
        // If this period is unsupported, report the actual open error instead
        // of changing the user's latency setting behind their back.
        setup.sampleRate = 0.0;
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

bool AudioDeviceService::hasSustainedRuntimeInstability()
{
    auto* device = manager.getCurrentAudioDevice();
    if (device == nullptr || ! device->isOpen()) return false;
    const auto xruns = manager.getXRunCount();
    if (xruns < 0) return false;
    const auto cpuOverloaded = manager.getCpuUsage() >= 0.82;
    // A counter increment alone does not establish audible loss.
    if (xruns > observedXRunCount && cpuOverloaded)
        ++unstablePolls;
    else
        unstablePolls = 0;
    observedXRunCount = xruns;
    if (unstablePolls < 3) return false;
    unstablePolls = 0;
    return true;
}

bool AudioDeviceService::isAutomaticMode()
{
    if (auto* settings = properties.getUserSettings())
        return settings->getValue("audioSetupMode") != "manual";
    return true;
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
