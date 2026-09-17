#include "AudioDeviceService.h"
#include <algorithm>

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

juce::String AudioDeviceService::selectDeviceType(const juce::String& typeName)
{
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
    auto setup = manager.getAudioDeviceSetup();
    setup.outputDeviceName = outputName;
    setup.inputDeviceName.clear();
    setup.sampleRate = sampleRate;
    setup.bufferSize = bufferSize;
    setup.useDefaultInputChannels = false;
    setup.useDefaultOutputChannels = true;
    lastError = manager.setAudioDeviceSetup(setup, true);
    if (lastError.isEmpty()) saveSettings();
    return lastError;
}

bool AudioDeviceService::followSystemDefaultOutput()
{
   #if JUCE_WINDOWS
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

    auto setup = manager.getAudioDeviceSetup();
    setup.inputDeviceName.clear();
    setup.useDefaultInputChannels = false;
    setup.useDefaultOutputChannels = true;

    const auto rates = device->getAvailableSampleRates();
    if (rates.contains(48000.0))
        setup.sampleRate = 48000.0;

    auto buffers = device->getAvailableBufferSizes();
    std::sort(buffers.begin(), buffers.end());
    const auto preferred = device->getTypeName().containsIgnoreCase("ASIO") ? 64 : 128;
    int chosen = setup.bufferSize > 0 ? setup.bufferSize : preferred;
    for (const auto candidate : buffers)
        if (candidate >= preferred) { chosen = candidate; break; }
    // 用户已经选择了更低且设备支持的缓冲时不把它调大。
    if (setup.bufferSize > 0 && setup.bufferSize < chosen && buffers.contains(setup.bufferSize))
        chosen = setup.bufferSize;
    setup.bufferSize = chosen;

    lastError = manager.setAudioDeviceSetup(setup, true);
    observedXRunCount = manager.getXRunCount();
    unstablePolls = 0;
    if (lastError.isNotEmpty())
        return lastError;
    saveSettings();
    return juce::String::fromUTF8("实时演奏模式已启用：48 kHz、关闭无用输入、缓冲区 ")
         + juce::String(chosen) + juce::String::fromUTF8(" 采样");
}

bool AudioDeviceService::stabiliseAfterXRuns()
{
    auto* device = manager.getCurrentAudioDevice();
    if (device == nullptr || ! device->isOpen()) return false;
    const auto xruns = manager.getXRunCount();
    if (xruns < 0) return false;
    if (xruns > observedXRunCount)
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
