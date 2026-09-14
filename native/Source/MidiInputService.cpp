#include "MidiInputService.h"

namespace fengyin
{
MidiInputService::MidiInputService()
{
    juce::PropertiesFile::Options options;
    options.applicationName = "FengYinMidi";
    options.filenameSuffix = ".settings";
    options.folderName = "FengYin";
    options.osxLibrarySubFolder = "Application Support";
    properties.setStorageParameters(options);
    BreathMapper::Settings settings;
    settings.threshold = 0.02f;
    settings.curve = 0.9f;
    settings.smoothing = 0.28f;
    breathMapper.setSettings(settings);
}

MidiInputService::~MidiInputService()
{
    disconnect();
}

std::vector<juce::MidiDeviceInfo> MidiInputService::getAvailableDevices() const
{
    const auto devices = juce::MidiInput::getAvailableDevices();
    return { devices.begin(), devices.end() };
}

bool MidiInputService::connect(const juce::String& identifier)
{
    disconnect();
    for (const auto& device : juce::MidiInput::getAvailableDevices())
    {
        if (device.identifier != identifier)
            continue;

        input = juce::MidiInput::openDevice(identifier, this);
        if (input == nullptr)
            return false;

        connectedName = device.name;
        connectedIdentifier = device.identifier;
        activeProfile = DeviceProfileMatcher::match(device.name.toStdString());
        auto savedController = activeProfile.breathController;
        if (auto* settingsFile = properties.getUserSettings())
            savedController = settingsFile->getIntValue(controllerSettingKey(), savedController);
        breathController.store(juce::jlimit(0, 127, savedController));
        activeProfile.breathController = breathController.load();
        BreathMapper::Settings settings;
        settings.threshold = 0.02f;
        settings.curve = activeProfile.breathCurve;
        settings.smoothing = activeProfile.smoothing;
        if (auto* settingsFile = properties.getUserSettings())
        {
            settings.threshold = static_cast<float>(settingsFile->getDoubleValue(controllerSettingKey() + ".threshold", settings.threshold));
            settings.curve = static_cast<float>(settingsFile->getDoubleValue(controllerSettingKey() + ".curve", settings.curve));
            settings.smoothing = static_cast<float>(settingsFile->getDoubleValue(controllerSettingKey() + ".smoothing", settings.smoothing));
            pitchSensitivity.store(static_cast<float>(settingsFile->getDoubleValue(controllerSettingKey() + ".pitch", 1.0)));
        }
        { const juce::SpinLock::ScopedLockType lock(breathMapperLock); breathMapper.setSettings(settings); }
        input->start();
        savePreferences();
        return true;
    }
    return false;
}

void MidiInputService::disconnect()
{
    if (input != nullptr)
        input->stop();
    input.reset();
    connectedName.clear();
    connectedIdentifier.clear();
    lastNote.store(-1, std::memory_order_relaxed);
    velocity.store(0.0f, std::memory_order_relaxed);
    breath.store(0.0f, std::memory_order_relaxed);
}

void MidiInputService::refreshAndConnectFirstAvailable()
{
    if (input != nullptr)
        return;
    const auto devices = juce::MidiInput::getAvailableDevices();
    if (devices.isEmpty()) return;
    if (auto* settingsFile = properties.getUserSettings())
    {
        const auto preferred = settingsFile->getValue("preferredDevice");
        for (const auto& device : devices)
            if (device.identifier == preferred && connect(preferred)) return;
    }
    connect(devices.getFirst().identifier);
}

void MidiInputService::pollConnection()
{
    const auto devices = juce::MidiInput::getAvailableDevices();
    if (input != nullptr)
    {
        for (const auto& device : devices)
            if (device.identifier == connectedIdentifier)
                return;
        disconnect();
    }

    const auto preferred = properties.getUserSettings() != nullptr
        ? properties.getUserSettings()->getValue("preferredDevice") : juce::String();
    if (preferred.isNotEmpty())
    {
        for (const auto& device : devices)
            if (device.identifier == preferred)
            {
                connect(preferred);
                return;
            }
        return;
    }
    if (! devices.isEmpty())
        connect(devices.getFirst().identifier);
}

MidiSnapshot MidiInputService::getSnapshot() const noexcept
{
    return {
        input != nullptr,
        lastNote.load(std::memory_order_relaxed),
        velocity.load(std::memory_order_relaxed),
        breath.load(std::memory_order_relaxed),
        pitchBend.load(std::memory_order_relaxed),
        messageCount.load(std::memory_order_relaxed)
    };
}

juce::String MidiInputService::getConnectedDeviceName() const
{
    return connectedName;
}

DeviceProfile MidiInputService::getActiveProfile() const
{
    return activeProfile;
}

void MidiInputService::setBreathController(int controllerNumber)
{
    const auto value = juce::jlimit(0, 127, controllerNumber);
    breathController.store(value);
    activeProfile.breathController = value;
    savePreferences();
}

juce::String MidiInputService::controllerSettingKey() const
{
    return "breathCC." + juce::String::toHexString(connectedIdentifier.hashCode64());
}

void MidiInputService::savePreferences()
{
    if (connectedIdentifier.isEmpty()) return;
    if (auto* settingsFile = properties.getUserSettings())
    {
        settingsFile->setValue("preferredDevice", connectedIdentifier);
        settingsFile->setValue(controllerSettingKey(), breathController.load());
        const auto expression = getExpressionSettings();
        settingsFile->setValue(controllerSettingKey() + ".threshold", expression.threshold);
        settingsFile->setValue(controllerSettingKey() + ".curve", expression.curve);
        settingsFile->setValue(controllerSettingKey() + ".smoothing", expression.smoothing);
        settingsFile->setValue(controllerSettingKey() + ".pitch", expression.pitchSensitivity);
        settingsFile->saveIfNeeded();
    }
}

ExpressionSettings MidiInputService::getExpressionSettings() const
{
    const juce::SpinLock::ScopedLockType lock(breathMapperLock);
    const auto settings = breathMapper.getSettings();
    return { settings.threshold, settings.curve, settings.smoothing, pitchSensitivity.load(std::memory_order_relaxed) };
}

void MidiInputService::setExpressionSettings(ExpressionSettings settings)
{
    BreathMapper::Settings mapped { settings.threshold, settings.curve, settings.smoothing };
    { const juce::SpinLock::ScopedLockType lock(breathMapperLock); breathMapper.setSettings(mapped); }
    activeProfile.breathCurve = juce::jlimit(0.25f, 4.0f, settings.curve);
    activeProfile.smoothing = juce::jlimit(0.01f, 1.0f, settings.smoothing);
    pitchSensitivity.store(juce::jlimit(0.25f, 2.0f, settings.pitchSensitivity), std::memory_order_relaxed);
    savePreferences();
}

void MidiInputService::setPerformanceSink(MidiPerformanceSink* sink) noexcept
{
    performanceSink.store(sink, std::memory_order_release);
}

void MidiInputService::beginBreathDetection() noexcept
{
    const juce::SpinLock::ScopedLockType lock(detectorLock);
    controllerDetector.reset();
    detectingBreath.store(true, std::memory_order_release);
}

int MidiInputService::finishBreathDetection() noexcept
{
    detectingBreath.store(false, std::memory_order_release);
    const juce::SpinLock::ScopedLockType lock(detectorLock);
    const auto detected = controllerDetector.bestContinuousController();
    if (detected >= 0)
        setBreathController(detected);
    return detected;
}

void MidiInputService::handleIncomingMidiMessage(juce::MidiInput*, const juce::MidiMessage& message)
{
    messageCount.fetch_add(1, std::memory_order_relaxed);

    if (message.isNoteOn())
    {
        lastNote.store(message.getNoteNumber(), std::memory_order_relaxed);
        velocity.store(message.getFloatVelocity(), std::memory_order_relaxed);
        if (auto* sink = performanceSink.load(std::memory_order_acquire))
            sink->noteOn(message.getNoteNumber(), message.getFloatVelocity());
    }
    else if (message.isNoteOff())
    {
        velocity.store(0.0f, std::memory_order_relaxed);
        if (auto* sink = performanceSink.load(std::memory_order_acquire))
            sink->noteOff(message.getNoteNumber());
    }
    else if (message.isController() && message.getControllerNumber() == breathController.load())
    {
        float mappedBreath = 0.0f;
        { const juce::SpinLock::ScopedLockType lock(breathMapperLock); mappedBreath = breathMapper.processMidiValue(message.getControllerValue()); }
        breath.store(mappedBreath, std::memory_order_relaxed);
        if (auto* sink = performanceSink.load(std::memory_order_acquire))
            sink->breathChanged(mappedBreath);
    }
    else if (message.isPitchWheel())
    {
        const auto centred = static_cast<float>(message.getPitchWheelValue() - 8192) / 8192.0f
                           * pitchSensitivity.load(std::memory_order_relaxed);
        pitchBend.store(juce::jlimit(-1.0f, 1.0f, centred), std::memory_order_relaxed);
        if (auto* sink = performanceSink.load(std::memory_order_acquire))
            sink->pitchBendChanged(juce::jlimit(-1.0f, 1.0f, centred));
    }
    if (message.isController() && detectingBreath.load(std::memory_order_acquire))
    {
        const juce::SpinLock::ScopedTryLockType lock(detectorLock);
        if (lock.isLocked())
            controllerDetector.observe(message.getControllerNumber(), message.getControllerValue());
    }
}
}
