#include "MidiInputService.h"

namespace fengyin
{
MidiInputService::MidiInputService()
{
    activeOutputNotes.fill(-1);
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
    {
        const juce::SpinLock::ScopedLockType lock(noteMapLock);
        if (auto* sink = performanceSink.load(std::memory_order_acquire))
            for (auto& outputNote : activeOutputNotes)
                if (outputNote >= 0) sink->noteOff(outputNote);
        activeOutputNotes.fill(-1);
    }
    if (auto* sink = performanceSink.load(std::memory_order_acquire))
        for (int technique = 0; technique < static_cast<int>(PerformanceTechnique::count); ++technique)
            sink->techniqueChanged(static_cast<PerformanceTechnique>(technique), 0.0f);
    connectedName.clear();
    connectedIdentifier.clear();
    lastNote.store(-1, std::memory_order_relaxed);
    velocity.store(0.0f, std::memory_order_relaxed);
    breath.store(0.0f, std::memory_order_relaxed);
    pitchBend.store(0.0f, std::memory_order_relaxed);
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

void MidiInputService::setTransposeSemitones(int semitones)
{
    const auto next = juce::jlimit(-12, 12, semitones);
    if (transposeSemitones.exchange(next, std::memory_order_relaxed) == next)
        return;

    const juce::SpinLock::ScopedLockType lock(noteMapLock);
    if (auto* sink = performanceSink.load(std::memory_order_acquire))
        for (auto& outputNote : activeOutputNotes)
            if (outputNote >= 0)
            {
                sink->noteOff(outputNote);
                outputNote = -1;
            }
    lastNote.store(-1, std::memory_order_relaxed);
    velocity.store(0.0f, std::memory_order_relaxed);
}

void MidiInputService::setTechniqueMappings(const juce::Array<TechniqueMapping>& mappings)
{
    {
        const juce::SpinLock::ScopedLockType lock(techniqueLock);
        techniqueMappings = mappings;
        techniquePreviousInput.fill(0.0f);
        techniqueToggleState.fill(false);
    }
    if (auto* sink = performanceSink.load(std::memory_order_acquire))
        for (int technique = 0; technique < static_cast<int>(PerformanceTechnique::count); ++technique)
            sink->techniqueChanged(static_cast<PerformanceTechnique>(technique), 0.0f);
}

juce::Array<TechniqueMapping> MidiInputService::getTechniqueMappings() const
{
    const juce::SpinLock::ScopedLockType lock(techniqueLock);
    return techniqueMappings;
}

void MidiInputService::beginTechniqueLearn(PerformanceTechnique technique) noexcept
{
    learnedSourceReady.store(false, std::memory_order_relaxed);
    learnedSourceNumber.store(-1, std::memory_order_relaxed);
    learningTechniqueId.store(static_cast<int>(technique), std::memory_order_relaxed);
    learningTechnique.store(true, std::memory_order_release);
}

void MidiInputService::cancelTechniqueLearn() noexcept
{
    learningTechnique.store(false, std::memory_order_release);
    learnedSourceReady.store(false, std::memory_order_relaxed);
}

TechniqueLearnResult MidiInputService::consumeTechniqueLearnResult() noexcept
{
    if (! learnedSourceReady.exchange(false, std::memory_order_acq_rel))
        return {};
    TechniqueLearnResult result;
    result.ready = true;
    result.mapping.technique = static_cast<PerformanceTechnique>(learningTechniqueId.load(std::memory_order_relaxed));
    result.mapping.sourceType = static_cast<TechniqueSourceType>(learnedSourceType.load(std::memory_order_relaxed));
    result.mapping.sourceNumber = learnedSourceNumber.load(std::memory_order_relaxed);
    return result;
}

float MidiInputService::techniqueMessageValue(const juce::MidiMessage& message) noexcept
{
    if (message.isController()) return static_cast<float>(message.getControllerValue()) / 127.0f;
    if (message.isChannelPressure()) return static_cast<float>(message.getChannelPressureValue()) / 127.0f;
    if (message.isPitchWheel()) return static_cast<float>(message.getPitchWheelValue()) / 16383.0f;
    if (message.isNoteOn()) return message.getFloatVelocity();
    if (message.isNoteOff()) return 0.0f;
    return 0.0f;
}

bool MidiInputService::handleTechniqueMessage(const juce::MidiMessage& message, MidiPerformanceSink* sink)
{
    TechniqueSourceType sourceType = TechniqueSourceType::none;
    int sourceNumber = -1;
    if (message.isController())
    {
        sourceType = TechniqueSourceType::controller;
        sourceNumber = message.getControllerNumber();
    }
    else if (message.isChannelPressure()) sourceType = TechniqueSourceType::channelPressure;
    else if (message.isPitchWheel()) sourceType = TechniqueSourceType::pitchWheel;
    else if (message.isNoteOnOrOff())
    {
        sourceType = TechniqueSourceType::note;
        sourceNumber = message.getNoteNumber();
    }

    if (sourceType == TechniqueSourceType::none)
        return false;

    bool capturedByLearn = false;
    if (learningTechnique.load(std::memory_order_acquire)
        && ! (sourceType == TechniqueSourceType::controller && sourceNumber == breathController.load()))
    {
        learnedSourceType.store(static_cast<int>(sourceType), std::memory_order_relaxed);
        learnedSourceNumber.store(sourceNumber, std::memory_order_relaxed);
        learningTechnique.store(false, std::memory_order_release);
        learnedSourceReady.store(true, std::memory_order_release);
        capturedByLearn = true;
    }

    const auto inputValue = techniqueMessageValue(message);
    bool consumedMessage = false;
    const juce::SpinLock::ScopedTryLockType lock(techniqueLock);
    if (! lock.isLocked()) return false;
    for (const auto& mapping : techniqueMappings)
    {
        if (mapping.sourceType != sourceType || (sourceType == TechniqueSourceType::controller && mapping.sourceNumber != sourceNumber)
            || (sourceType == TechniqueSourceType::note && mapping.sourceNumber != sourceNumber))
            continue;
        const auto index = static_cast<size_t>(mapping.technique);
        auto outputValue = inputValue;
        if (mapping.toggle)
        {
            if (inputValue > 0.5f && techniquePreviousInput[index] <= 0.5f)
                techniqueToggleState[index] = ! techniqueToggleState[index];
            outputValue = techniqueToggleState[index] ? 1.0f : 0.0f;
        }
        techniquePreviousInput[index] = inputValue;
        if (sink != nullptr) sink->techniqueChanged(mapping.technique, outputValue);
        consumedMessage = true;
    }
    return consumedMessage || capturedByLearn;
}

void MidiInputService::handleIncomingMidiMessage(juce::MidiInput*, const juce::MidiMessage& message)
{
    messageCount.fetch_add(1, std::memory_order_relaxed);
    auto* sink = performanceSink.load(std::memory_order_acquire);
    if (handleTechniqueMessage(message, sink))
        return;

    if (message.isNoteOn())
    {
        const auto sourceNote = message.getNoteNumber();
        const auto outputNote = juce::jlimit(0, 127, sourceNote + transposeSemitones.load(std::memory_order_relaxed));
        { const juce::SpinLock::ScopedLockType lock(noteMapLock); activeOutputNotes[static_cast<size_t>(sourceNote)] = outputNote; }
        lastNote.store(outputNote, std::memory_order_relaxed);
        velocity.store(message.getFloatVelocity(), std::memory_order_relaxed);
        if (sink != nullptr) sink->noteOn(outputNote, message.getFloatVelocity());
    }
    else if (message.isNoteOff())
    {
        auto outputNote = juce::jlimit(0, 127, message.getNoteNumber() + transposeSemitones.load(std::memory_order_relaxed));
        { const juce::SpinLock::ScopedLockType lock(noteMapLock);
          const auto sourceNote = static_cast<size_t>(message.getNoteNumber());
          if (activeOutputNotes[sourceNote] >= 0) outputNote = activeOutputNotes[sourceNote];
          activeOutputNotes[sourceNote] = -1; }
        velocity.store(0.0f, std::memory_order_relaxed);
        if (sink != nullptr) sink->noteOff(outputNote);
    }
    else if (message.isController() && message.getControllerNumber() == breathController.load())
    {
        float mappedBreath = 0.0f;
        { const juce::SpinLock::ScopedLockType lock(breathMapperLock); mappedBreath = breathMapper.processMidiValue(message.getControllerValue()); }
        breath.store(mappedBreath, std::memory_order_relaxed);
        if (sink != nullptr) sink->breathChanged(mappedBreath);
    }
    else if (message.isPitchWheel())
    {
        const auto centred = static_cast<float>(message.getPitchWheelValue() - 8192) / 8192.0f
                           * pitchSensitivity.load(std::memory_order_relaxed);
        pitchBend.store(juce::jlimit(-1.0f, 1.0f, centred), std::memory_order_relaxed);
        if (sink != nullptr) sink->pitchBendChanged(juce::jlimit(-1.0f, 1.0f, centred));
    }
    if (message.isController() && detectingBreath.load(std::memory_order_acquire))
    {
        const juce::SpinLock::ScopedTryLockType lock(detectorLock);
        if (lock.isLocked())
            controllerDetector.observe(message.getControllerNumber(), message.getControllerValue());
    }
}
}
