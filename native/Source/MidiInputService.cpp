#include "MidiInputService.h"

namespace fengyin
{
MidiInputService::MidiInputService()
{
    activeOutputNotes.fill(-1);
    techniqueHardwareInput.fill(0.0f);
    techniqueBreathInput.fill(0.0f);
    juce::PropertiesFile::Options options;
    options.applicationName = "FengYinMidi";
    options.filenameSuffix = ".settings";
    options.folderName = "FengYin";
    options.osxLibrarySubFolder = "Application Support";
    properties.setStorageParameters(options);
    if (auto* settingsFile = properties.getUserSettings())
        targetKey.store(PitchKey::normalise(settingsFile->getIntValue("targetPerformanceKey", 0)));
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
        {
            savedController = settingsFile->getIntValue(controllerSettingKey(), savedController);
            // 0.5.x 曾把 Yamaha YDS 的出厂气息控制误存为 CC2。
            // 首次升级时迁移到官方默认 CC11；之后仍允许用户手动修改并记忆。
            const auto profileRevisionKey = controllerSettingKey() + ".profileRevision";
            if (activeProfile.id == "yamaha-yds" && settingsFile->getIntValue(profileRevisionKey, 0) < 1)
            {
                savedController = 11;
                settingsFile->setValue(profileRevisionKey, 1);
            }
        }
        updateEffectiveTranspose();
        breathController.store(juce::jlimit(0, 127, savedController));
        activeProfile.breathController = breathController.load();
        if (auto* settingsFile = properties.getUserSettings())
        {
            pitchSensitivity.store(static_cast<float>(settingsFile->getDoubleValue(controllerSettingKey() + ".pitch", 1.0)));
            const auto savedSensitivity = juce::jlimit(0, 2,
                settingsFile->getIntValue(controllerSettingKey() + ".growlSensitivity", 1));
            growlSensitivity.store(savedSensitivity, std::memory_order_relaxed);
            if (savedSensitivity == 0) intelligentTechniques.setGrowlThresholds(0.82f, 0.74f);
            else if (savedSensitivity == 2) intelligentTechniques.setGrowlThresholds(0.94f, 0.86f);
            else intelligentTechniques.setGrowlThresholds(0.89f, 0.80f);
        }
        loadTechniqueMappings();
        intelligentTechniques.reset();
        activeNoteCount = 0;
        input->start();
        if (auto* sink = performanceSink.load(std::memory_order_acquire))
            sink->resetPerformance();
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
        sink->resetPerformance();
    connectedName.clear();
    connectedIdentifier.clear();
    activeProfile = DeviceProfileMatcher::match("");
    breathController.store(activeProfile.breathController, std::memory_order_relaxed);
    lastNote.store(-1, std::memory_order_relaxed);
    velocity.store(0.0f, std::memory_order_relaxed);
    breath.store(0.0f, std::memory_order_relaxed);
    pitchBend.store(0.0f, std::memory_order_relaxed);
    intelligentTechniques.reset();
    activeNoteCount = 0;
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
    }
    // 上次使用的设备不在时仍应接入当前唯一/首个电吹管，避免换品牌后界面一直显示未连接。
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

juce::String MidiInputService::techniqueSettingKey() const
{
    return "technique.roles.v2." + juce::String::toHexString(connectedIdentifier.hashCode64());
}

void MidiInputService::savePreferences()
{
    if (connectedIdentifier.isEmpty()) return;
    if (auto* settingsFile = properties.getUserSettings())
    {
        settingsFile->setValue("preferredDevice", connectedIdentifier);
        settingsFile->setValue(controllerSettingKey(), breathController.load());
        settingsFile->setValue(controllerSettingKey() + ".pitch", pitchSensitivity.load(std::memory_order_relaxed));
        settingsFile->setValue(controllerSettingKey() + ".growlSensitivity", growlSensitivity.load(std::memory_order_relaxed));
        settingsFile->setValue("targetPerformanceKey", targetKey.load(std::memory_order_relaxed));
        settingsFile->saveIfNeeded();
    }
}

ExpressionSettings MidiInputService::getExpressionSettings() const
{
    return { 0.0f, 1.0f, 1.0f, pitchSensitivity.load(std::memory_order_relaxed) };
}

void MidiInputService::setExpressionSettings(ExpressionSettings settings)
{
    pitchSensitivity.store(juce::jlimit(0.25f, 2.0f, settings.pitchSensitivity), std::memory_order_relaxed);
    savePreferences();
}

void MidiInputService::setPerformanceSink(MidiPerformanceSink* sink) noexcept
{
    performanceSink.store(sink, std::memory_order_release);
    if (sink != nullptr)
        sink->resetPerformance();
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

void MidiInputService::setTargetKey(int pitchClass)
{
    targetKey.store(PitchKey::normalise(pitchClass), std::memory_order_relaxed);
    updateEffectiveTranspose();
    if (auto* settingsFile = properties.getUserSettings())
    {
        settingsFile->setValue("targetPerformanceKey", targetKey.load(std::memory_order_relaxed));
        settingsFile->saveIfNeeded();
    }
}

void MidiInputService::setTransposeSemitones(int semitones)
{
    setTargetKey(semitones);
}

void MidiInputService::updateEffectiveTranspose()
{
    // 风吟统一要求电吹管本机设为 C 调，软件只负责从 C 调换算到目标演奏调。
    const auto next = PitchKey::transposeFromTo(0, targetKey.load(std::memory_order_relaxed));
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

void MidiInputService::setGrowlSensitivity(int level)
{
    const auto safeLevel = juce::jlimit(0, 2, level);
    growlSensitivity.store(safeLevel, std::memory_order_relaxed);
    if (safeLevel == 0) intelligentTechniques.setGrowlThresholds(0.82f, 0.74f);
    else if (safeLevel == 2) intelligentTechniques.setGrowlThresholds(0.94f, 0.86f);
    else intelligentTechniques.setGrowlThresholds(0.89f, 0.80f);
    savePreferences();
}

void MidiInputService::setTechniqueMappings(const juce::Array<TechniqueMapping>& mappings)
{
    {
        const juce::SpinLock::ScopedLockType lock(techniqueLock);
        techniqueMappings = mappings;
        techniquePreviousInput.fill(0.0f);
        techniqueToggleState.fill(false);
        techniqueHardwareInput.fill(0.0f);
        techniqueBreathInput.fill(0.0f);
    }
    if (auto* sink = performanceSink.load(std::memory_order_acquire))
        for (int technique = 0; technique < static_cast<int>(PerformanceTechnique::count); ++technique)
            sink->techniqueChanged(static_cast<PerformanceTechnique>(technique), 0.0f);
    saveTechniqueMappings();
}

juce::Array<TechniqueMapping> MidiInputService::getTechniqueMappings() const
{
    const juce::SpinLock::ScopedLockType lock(techniqueLock);
    return techniqueMappings;
}

void MidiInputService::setTechniqueContext(const juce::String& instrumentFamily)
{
    const auto next = instrumentFamily.isNotEmpty() ? instrumentFamily : juce::String("other");
    techniqueContext = next;
}

PerformanceTechnique MidiInputService::targetForRole(PerformanceTechnique role) const noexcept
{
    if (role == PerformanceTechnique::vibrato) return PerformanceTechnique::vibrato;
    if (role == PerformanceTechnique::portamento) return PerformanceTechnique::portamento;
    if (role == PerformanceTechnique::growl)
    {
        if (techniqueContext == "woodwind") return PerformanceTechnique::flutter;
        if (techniqueContext == "strings") return PerformanceTechnique::tremolo;
        return PerformanceTechnique::growl;
    }
    if (role == PerformanceTechnique::mute)
    {
        if (techniqueContext == "strings") return PerformanceTechnique::pizzicato;
        if (techniqueContext == "woodwind" || techniqueContext == "saxophone")
            return PerformanceTechnique::alternateFingering;
        return PerformanceTechnique::mute;
    }
    return role;
}

void MidiInputService::saveTechniqueMappings()
{
    if (connectedIdentifier.isEmpty() || properties.getUserSettings() == nullptr)
        return;
    juce::String serialised;
    for (const auto& mapping : getTechniqueMappings())
    {
        if (serialised.isNotEmpty()) serialised += ";";
        serialised += juce::String(static_cast<int>(mapping.technique)) + ","
                   + juce::String(static_cast<int>(mapping.sourceType)) + ","
                   + juce::String(mapping.sourceNumber) + ","
                   + juce::String(mapping.toggle ? 1 : 0) + ","
                   + juce::String(static_cast<int>(mapping.mode)) + ","
                   + juce::String(mapping.strength, 4);
    }
    properties.getUserSettings()->setValue(techniqueSettingKey(), serialised);
    properties.getUserSettings()->saveIfNeeded();
}

void MidiInputService::loadTechniqueMappings()
{
    juce::Array<TechniqueMapping> loaded;
    if (connectedIdentifier.isNotEmpty() && properties.getUserSettings() != nullptr)
    {
        juce::StringArray entries;
        entries.addTokens(properties.getUserSettings()->getValue(techniqueSettingKey()), ";", "");
        for (const auto& entry : entries)
        {
            juce::StringArray fields;
            fields.addTokens(entry, ",", "");
            if (fields.size() != 4 && fields.size() != 6) continue;
            TechniqueMapping mapping;
            mapping.technique = static_cast<PerformanceTechnique>(juce::jlimit(0, static_cast<int>(PerformanceTechnique::count) - 1, fields[0].getIntValue()));
            mapping.sourceType = static_cast<TechniqueSourceType>(juce::jlimit(0, static_cast<int>(TechniqueSourceType::note), fields[1].getIntValue()));
            mapping.sourceNumber = fields[2].getIntValue();
            mapping.toggle = fields[3].getIntValue() != 0;
            if (fields.size() >= 6)
            {
                mapping.mode = static_cast<TechniqueControlMode>(juce::jlimit(0, static_cast<int>(TechniqueControlMode::off), fields[4].getIntValue()));
                mapping.strength = juce::jlimit(0.0f, 1.0f, fields[5].getFloatValue());
            }
            else
                mapping.mode = TechniqueControlMode::hardware;
            loaded.add(mapping);
        }
    }
    {
        const juce::SpinLock::ScopedLockType lock(techniqueLock);
        techniqueMappings = loaded;
        techniquePreviousInput.fill(0.0f);
        techniqueToggleState.fill(false);
        techniqueHardwareInput.fill(0.0f);
        techniqueBreathInput.fill(0.0f);
    }
    if (auto* sink = performanceSink.load(std::memory_order_acquire))
        for (int technique = 0; technique < static_cast<int>(PerformanceTechnique::count); ++technique)
            sink->techniqueChanged(static_cast<PerformanceTechnique>(technique), 0.0f);
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
    const auto inputValue = techniqueMessageValue(message);
    const auto ordinaryPlayedNote = sourceType == TechniqueSourceType::note
        && breath.load(std::memory_order_relaxed) > 0.03f;
    const auto meaningfulContinuousSignal = sourceType == TechniqueSourceType::pitchWheel
        ? std::abs(inputValue - 0.5f) > 0.18f : inputValue > 0.55f;
    const auto validLearningSignal = sourceType == TechniqueSourceType::note
        ? message.isNoteOn() && ! ordinaryPlayedNote : meaningfulContinuousSignal;
    if (learningTechnique.load(std::memory_order_acquire) && validLearningSignal
        && ! (sourceType == TechniqueSourceType::controller && sourceNumber == breathController.load()))
    {
        learnedSourceType.store(static_cast<int>(sourceType), std::memory_order_relaxed);
        learnedSourceNumber.store(sourceNumber, std::memory_order_relaxed);
        learningTechnique.store(false, std::memory_order_release);
        learnedSourceReady.store(true, std::memory_order_release);
        capturedByLearn = true;
    }

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
        techniqueHardwareInput[index] = outputValue;
        if (sink != nullptr && mapping.mode != TechniqueControlMode::off && mapping.mode != TechniqueControlMode::breath)
        {
            const auto scaled = outputValue * juce::jlimit(0.0f, 1.0f, mapping.strength * 1.35f);
            sink->techniqueChanged(targetForRole(mapping.technique),
                mapping.mode == TechniqueControlMode::hybrid ? scaled * techniqueBreathInput[index] : scaled);
        }
        consumedMessage = true;
    }
    return consumedMessage || capturedByLearn;
}

void MidiInputService::updateBreathDrivenTechniques(float mappedBreath, MidiPerformanceSink* sink, double nowMs)
{
    if (sink == nullptr) return;
    const juce::SpinLock::ScopedTryLockType lock(techniqueLock);
    if (! lock.isLocked()) return;
    for (const auto& mapping : techniqueMappings)
    {
        if (mapping.mode == TechniqueControlMode::off || mapping.mode == TechniqueControlMode::hardware)
            continue;
        if (! IntelligentTechniqueProcessor::canUseBreath(mapping.technique)) continue;
        const auto index = static_cast<size_t>(mapping.technique);
        const auto value = intelligentTechniques.process(mapping.technique, mappedBreath, mapping.strength, nowMs);
        techniqueBreathInput[index] = value;
        sink->techniqueChanged(targetForRole(mapping.technique),
            mapping.mode == TechniqueControlMode::hybrid ? value * techniqueHardwareInput[index] : value);
    }
}

void MidiInputService::handleIncomingMidiMessage(juce::MidiInput*, const juce::MidiMessage& message)
{
    messageCount.fetch_add(1, std::memory_order_relaxed);
    auto* sink = performanceSink.load(std::memory_order_acquire);
    // JUCE MIDI backends provide a monotonic arrival timestamp. Preserve it all the
    // way to the audio callback so the collector can place the event at the correct
    // sample offset instead of quantising every event to the next block boundary.
    const auto timestampSeconds = message.getTimeStamp() > 0.0
        ? message.getTimeStamp() : juce::Time::getMillisecondCounterHiRes() * 0.001;
    if (handleTechniqueMessage(message, sink))
        return;

    if (message.isNoteOn())
    {
        const auto sourceNote = message.getNoteNumber();
        const auto outputNote = juce::jlimit(0, 127, sourceNote + transposeSemitones.load(std::memory_order_relaxed));
        { const juce::SpinLock::ScopedLockType lock(noteMapLock); activeOutputNotes[static_cast<size_t>(sourceNote)] = outputNote; }
        lastNote.store(outputNote, std::memory_order_relaxed);
        const auto currentBreath = breath.load(std::memory_order_relaxed);
        auto safeVelocity = message.getFloatVelocity();
        if (sink != nullptr) sink->breathChanged(currentBreath, timestampSeconds);
        velocity.store(safeVelocity, std::memory_order_relaxed);
        if (activeNoteCount++ == 0)
            intelligentTechniques.noteStarted(juce::Time::getMillisecondCounterHiRes());
        if (sink != nullptr) sink->noteOn(outputNote, safeVelocity, timestampSeconds);
    }
    else if (message.isNoteOff())
    {
        auto outputNote = juce::jlimit(0, 127, message.getNoteNumber() + transposeSemitones.load(std::memory_order_relaxed));
        { const juce::SpinLock::ScopedLockType lock(noteMapLock);
          const auto sourceNote = static_cast<size_t>(message.getNoteNumber());
          if (activeOutputNotes[sourceNote] >= 0) outputNote = activeOutputNotes[sourceNote];
          activeOutputNotes[sourceNote] = -1; }
        velocity.store(0.0f, std::memory_order_relaxed);
        if (activeNoteCount > 0 && --activeNoteCount == 0)
            intelligentTechniques.noteEnded();
        if (sink != nullptr) sink->noteOff(outputNote, timestampSeconds);
    }
    else if (message.isController() && message.getControllerNumber() == breathController.load())
    {
        const auto rawBreath = static_cast<float>(message.getControllerValue()) / 127.0f;
        breath.store(rawBreath, std::memory_order_relaxed);
        if (sink != nullptr)
        {
            sink->breathChanged(rawBreath, timestampSeconds);
            updateBreathDrivenTechniques(rawBreath, sink, juce::Time::getMillisecondCounterHiRes());
        }
    }
    else if (message.isPitchWheel())
    {
        const auto raw = message.getPitchWheelValue();
        const auto centred = static_cast<float>(raw - 8192) / (raw < 8192 ? 8192.0f : 8191.0f);
        pitchBend.store(juce::jlimit(-1.0f, 1.0f, centred), std::memory_order_relaxed);
        if (sink != nullptr) sink->pitchBendChanged(juce::jlimit(-1.0f, 1.0f, centred), timestampSeconds);
    }
    if (message.isController() && detectingBreath.load(std::memory_order_acquire))
    {
        const juce::SpinLock::ScopedTryLockType lock(detectorLock);
        if (lock.isLocked())
            controllerDetector.observe(message.getControllerNumber(), message.getControllerValue());
    }
}
}
