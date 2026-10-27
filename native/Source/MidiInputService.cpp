#include "MidiInputService.h"
#include "TechniqueSignal.h"
#include "SimpleTechniquePolicy.h"

namespace fengyin
{
MidiInputService::MidiInputService(bool persistSettings)
{
    activeOutputNotes.fill(-1);
    techniqueHardwareInput.fill(0.0f);
    techniqueBreathInput.fill(0.0f);
    TechniqueMapping defaultVibrato;
    defaultVibrato.technique=PerformanceTechnique::vibrato;
    defaultVibrato.mode=TechniqueControlMode::breath;
    defaultVibrato.strength=1.0f;
    techniqueMappings.add(defaultVibrato);
    if (!persistSettings) return;
    juce::PropertiesFile::Options options;
    options.applicationName = "FengYinMidi";
    options.filenameSuffix = ".settings";
    options.folderName = "FengYin";
    options.osxLibrarySubFolder = "Application Support";
    properties.setStorageParameters(options);
    if (auto* settingsFile = properties.getUserSettings()) {
        targetKey.store(PitchKey::normalise(settingsFile->getIntValue("performanceKeyV3", 0)));
        musicTempoMode.store(MusicTempoPolicy::valid(settingsFile->getIntValue("musicTempoV1",1)));
    }
    updateEffectiveTranspose();
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

        prepareDevice(device.name, device.identifier);
        input->start();
        if (auto* sink = performanceSink.load(std::memory_order_acquire))
            sink->resetPerformance();
        savePreferences();
        return true;
    }
    return false;
}

void MidiInputService::prepareDevice(const juce::String& name, const juce::String& identifier)
{
    connectedName = name;
    connectedIdentifier = identifier;
    activeProfile = DeviceProfileMatcher::match(name.toStdString());
    restoreBreathController();
    updateEffectiveTranspose();
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
}

void MidiInputService::attachExternal(const juce::String& identifier, const juce::String& name)
{
    disconnect();
    prepareDevice(name, identifier);
    externalConnected = true;
    savePreferences();
}

void MidiInputService::acceptExternalMessage(const juce::MidiMessage& message)
{
    // The BLE service serialises delivery and stops it before lifecycle changes.
    if (externalConnected) handleIncomingMidiMessage(nullptr, message);
}

void MidiInputService::disconnect()
{
    if (input != nullptr)
        input->stop();
    input.reset();
    externalConnected = false;
    cancelBreathDetection();
    ++connectionGeneration;
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
    if (input != nullptr || externalConnected || automaticConnectionSuspended)
        return;
    const auto devices = juce::MidiInput::getAvailableDevices();
    availableDeviceCache.assign(devices.begin(), devices.end());
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
    availableDeviceCache.assign(devices.begin(), devices.end());
    // Refresh the selectable USB list even while BLE owns the active input.
    if (externalConnected) return;
    if (input != nullptr)
    {
        for (const auto& device : devices)
            if (device.identifier == connectedIdentifier)
                return;
        disconnect();
    }

    if (automaticConnectionSuspended) return;
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
        input != nullptr || externalConnected,
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

juce::String MidiInputService::getConnectedDeviceIdentifier() const
{
    return connectedIdentifier;
}

bool MidiInputService::hasCompletedBreathMatch()
{
    if (connectedIdentifier.isEmpty() || properties.getUserSettings() == nullptr)
        return false;
    return properties.getUserSettings()->getBoolValue(controllerSettingKey() + ".matched", false);
}

void MidiInputService::restoreBreathController()
{
    auto savedController = activeProfile.breathController;
    if (auto* settingsFile = properties.getUserSettings())
    {
        savedController = settingsFile->getIntValue(controllerSettingKey(), savedController);
        // Preserve the existing one-time migration for pre-0.5 Yamaha profiles.
        const auto profileRevisionKey = controllerSettingKey() + ".profileRevision";
        if (activeProfile.id == "yamaha-yds" && settingsFile->getIntValue(profileRevisionKey, 0) < 1)
        {
            savedController = 11;
            settingsFile->setValue(profileRevisionKey, 1);
        }
    }
    breathController.store(juce::jlimit(0, 127, savedController));
    activeProfile.breathController = breathController.load();
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
    if (connectedIdentifier.isNotEmpty() && properties.getUserSettings() != nullptr)
    {
        properties.getUserSettings()->setValue(controllerSettingKey() + ".matched", true);
        properties.getUserSettings()->saveIfNeeded();
    }
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
        settingsFile->setValue("performanceKeyV3", targetKey.load(std::memory_order_relaxed));
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

void MidiInputService::cancelBreathDetection() noexcept
{
    const juce::SpinLock::ScopedLockType lock(detectorLock);
    detectingBreath.store(false, std::memory_order_release);
    controllerDetector.reset();
}

int MidiInputService::finishBreathDetection(bool markCompleted) noexcept
{
    const juce::SpinLock::ScopedLockType lock(detectorLock);
    const auto wasDetecting = detectingBreath.exchange(false, std::memory_order_acq_rel);
    // Only an explicit, still-active user match may replace the saved controller.
    // A cancelled/disconnected or background observation must never commit.
    if (!wasDetecting || !markCompleted)
    {
        controllerDetector.reset();
        return -1;
    }
    const auto detected = controllerDetector.bestContinuousController();
    if (detected >= 0)
    {
        breathController.store(juce::jlimit(0, 127, detected), std::memory_order_relaxed);
        activeProfile.breathController = breathController.load(std::memory_order_relaxed);
        savePreferences();
        if (markCompleted && connectedIdentifier.isNotEmpty() && properties.getUserSettings() != nullptr)
        {
            properties.getUserSettings()->setValue(controllerSettingKey() + ".matched", true);
            properties.getUserSettings()->saveIfNeeded();
        }
    }
    return detected;
}

void MidiInputService::setMusicTempoMode(int mode)
{
    musicTempoMode.store(MusicTempoPolicy::valid(mode));
    if(auto* settings=properties.getUserSettings()) {settings->setValue("musicTempoV1",musicTempoMode.load());settings->saveIfNeeded();}
}

void MidiInputService::setTargetKey(int pitchClass)
{
    targetKey.store(PitchKey::normalise(pitchClass), std::memory_order_relaxed);
    updateEffectiveTranspose();
    if (auto* settingsFile = properties.getUserSettings())
    {
        settingsFile->setValue("performanceKeyV3", targetKey.load(std::memory_order_relaxed));
        settingsFile->saveIfNeeded();
    }
}

void MidiInputService::setTransposeSemitones(int semitones)
{
    setTargetKey(semitones);
}

void MidiInputService::updateEffectiveTranspose()
{
    // Offset applies to future Note On only. Keep the original note-off map
    // intact so held notes end at their original pitch, without a forced cut.
    transposeSemitones.store(PitchKey::transposeFromTo(0, targetKey.load(std::memory_order_relaxed)), std::memory_order_relaxed);
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
    const auto previous = getTechniqueMappings();
    {
        const juce::SpinLock::ScopedLockType lock(techniqueLock);
        techniqueMappings.clear();
        for (auto mapping : mappings)
        {
            if (mapping.sourceType == TechniqueSourceType::pitchWheel)
            {
                pitchMappingRemap.store(true);
                mapping.sourceType = TechniqueSourceType::none;
                mapping.sourceNumber = -1;
                mapping.mode = TechniqueControlMode::off;
            }
            techniqueMappings.add(mapping);
        }
        refreshTechniqueNoteMasks();
        techniquePreviousInput.fill(0.0f);
        techniqueToggleState.fill(false);
        techniqueHardwareInput.fill(0.0f);
        techniqueBreathInput.fill(0.0f);
    }
    if (auto* sink = performanceSink.load(std::memory_order_acquire))
        for (const auto& mapping : previous)
            sink->techniqueChanged(targetForRole(mapping.technique), 0.0f);
    saveTechniqueMappings();
}

void MidiInputService::refreshTechniqueNoteMasks() noexcept
{
    uint64_t masks[2] {};
    for (const auto& mapping : techniqueMappings)
        if (mapping.sourceType == TechniqueSourceType::note && mapping.sourceNumber >= 0 && mapping.sourceNumber < 128)
            masks[mapping.sourceNumber / 64] |= uint64_t(1) << (mapping.sourceNumber % 64);
    for (size_t i=0;i<2;++i) techniqueNoteMasks[i].store(masks[i], std::memory_order_release);
}

juce::Array<TechniqueMapping> MidiInputService::getTechniqueMappings() const
{
    const juce::SpinLock::ScopedLockType lock(techniqueLock);
    return techniqueMappings;
}

void MidiInputService::setTechniqueContext(const juce::String& instrumentFamily)
{
    const auto next = instrumentFamily.isNotEmpty() ? instrumentFamily : juce::String("other");
    {
        const juce::SpinLock::ScopedLockType lock(techniqueLock);
        if (auto* sink=performanceSink.load(std::memory_order_acquire))
            for (const auto& mapping:techniqueMappings)
                if (simpleRoleSupported(techniqueContext.toRawUTF8(),mapping.technique))
                    sink->techniqueChanged(targetForRole(mapping.technique),0.0f);
        techniqueContext = next;
    }
    if (next.startsWith("kong-")) onsetTrace.begin();
}

PerformanceTechnique MidiInputService::targetForRole(PerformanceTechnique role) const noexcept
{
    if (role == PerformanceTechnique::mute) return simpleOther(techniqueContext.toRawUTF8());
    if (role == PerformanceTechnique::vibrato) return PerformanceTechnique::vibrato;
    if (role == PerformanceTechnique::portamento) return PerformanceTechnique::portamento;
    if (role == PerformanceTechnique::growl)
    {
        if (techniqueContext == "kong-suona" || techniqueContext == "kong-dizi"
            || techniqueContext == "kong-sheng") return PerformanceTechnique::flutter;
        if (techniqueContext == "kong-guzheng" || techniqueContext == "kong-liuqin"
            || techniqueContext == "kong-pipa" || techniqueContext == "kong-sanxian")
            return PerformanceTechnique::tremolo;
        if (techniqueContext == "woodwind") return PerformanceTechnique::flutter;
        if (techniqueContext == "strings") return PerformanceTechnique::tremolo;
        return PerformanceTechnique::growl;
    }
    if (role == PerformanceTechnique::mute)
    {
        if (techniqueContext == "kong-dizi") return PerformanceTechnique::tremolo;
        if (techniqueContext == "kong-matouqin") return PerformanceTechnique::pizzicato;
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
    if (activeProfile.id != "generic-wind-controller" && connectedName.isNotEmpty())
        properties.getUserSettings()->setValue("technique.model.v3."+juce::String::toHexString(connectedName.hashCode64()),serialised);
    properties.getUserSettings()->saveIfNeeded();
}

void MidiInputService::loadTechniqueMappings()
{
    pitchMappingRemap.store(false);
    juce::Array<TechniqueMapping> loaded;
    if (connectedIdentifier.isNotEmpty() && properties.getUserSettings() != nullptr)
    {
        juce::StringArray entries;
        auto saved=properties.getUserSettings()->getValue(techniqueSettingKey());
        if (saved.isEmpty() && activeProfile.id != "generic-wind-controller" && connectedName.isNotEmpty())
            saved=properties.getUserSettings()->getValue("technique.model.v3."+juce::String::toHexString(connectedName.hashCode64()));
        entries.addTokens(saved, ";", "");
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
            if (mapping.sourceType == TechniqueSourceType::pitchWheel)
            {
                pitchMappingRemap.store(true);
                mapping.sourceType = TechniqueSourceType::none;
                mapping.sourceNumber = -1;
                mapping.mode = TechniqueControlMode::off;
                juce::Logger::writeToLog("Pitch Bend technique mapping disabled: remap to CC, pressure or button");
            }
            loaded.add(mapping);
        }
    }
    if(std::none_of(loaded.begin(),loaded.end(),[](const auto& mapping){return mapping.technique==PerformanceTechnique::vibrato;})) {
        TechniqueMapping mapping;
        mapping.technique=PerformanceTechnique::vibrato;
        mapping.mode=TechniqueControlMode::breath;
        mapping.strength=1.0f;
        loaded.add(mapping);
    }
    {
        const juce::SpinLock::ScopedLockType lock(techniqueLock);
        techniqueMappings = loaded;
        refreshTechniqueNoteMasks();
        techniquePreviousInput.fill(0.0f);
        techniqueToggleState.fill(false);
        techniqueHardwareInput.fill(0.0f);
        techniqueBreathInput.fill(0.0f);
    }
    if (pitchMappingRemap.load())
        saveTechniqueMappings();
    if (auto* sink = performanceSink.load(std::memory_order_acquire))
        for (const auto& mapping : loaded)
            if (mapping.mode != TechniqueControlMode::off)
                sink->techniqueChanged(targetForRole(mapping.technique), 0.0f);
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
    if (message.isPitchWheel()) return techniquePitchAmount(message.getPitchWheelValue());
    if (message.isNoteOn()) return message.getFloatVelocity();
    if (message.isNoteOff()) return 0.0f;
    return 0.0f;
}

bool MidiInputService::handleTechniqueMessage(const juce::MidiMessage& message, MidiPerformanceSink* sink)
{
    if (message.isPitchWheel()) return false; // Reserved for pitch, even during learning or lock contention.
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
    // Globally learned technique notes remain reserved even when a role is
    // unsupported/disabled/automatic. Otherwise they become audible notes.
    const bool reservedNote = sourceType == TechniqueSourceType::note
        && (techniqueNoteMasks[static_cast<size_t>(sourceNumber / 64)].load(std::memory_order_acquire)
            & (uint64_t(1) << (sourceNumber % 64))) != 0;
    const auto ordinaryPlayedNote = sourceType == TechniqueSourceType::note
        && breath.load(std::memory_order_relaxed) > 0.03f;
    const auto meaningfulContinuousSignal = sourceType == TechniqueSourceType::pitchWheel
        ? inputValue > 0.36f : inputValue > 0.55f;
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
    if (! lock.isLocked())
    {
        // Never let a potentially mapped wheel escape as an audible pitch bend
        // merely because the UI is updating its mapping. Notes still pass.
        return capturedByLearn || reservedNote;
    }
    for (const auto& mapping : techniqueMappings)
    {
        if (!simpleRoleSupported(techniqueContext.toRawUTF8(), mapping.technique)
            || mapping.mode != TechniqueControlMode::hardware) continue;
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
            const auto scaled = outputValue;
            sink->techniqueChanged(targetForRole(mapping.technique),
                mapping.mode == TechniqueControlMode::hybrid ? scaled * techniqueBreathInput[index] : scaled);
        }
        consumedMessage = true;
    }
    return consumedMessage || capturedByLearn || reservedNote;
}

void MidiInputService::updateBreathDrivenTechniques(float mappedBreath, MidiPerformanceSink* sink, double)
{
    if (sink == nullptr) return;
    const juce::SpinLock::ScopedTryLockType lock(techniqueLock);
    if (! lock.isLocked()) return;
    for (const auto& mapping : techniqueMappings)
    {
        if (!simpleRoleSupported(techniqueContext.toRawUTF8(), mapping.technique)) continue;
        if (mapping.technique != PerformanceTechnique::growl) continue;
        if (mapping.mode == TechniqueControlMode::off || mapping.mode == TechniqueControlMode::hardware)
            continue;
        if (! IntelligentTechniqueProcessor::canUseBreath(mapping.technique)) continue;
        const auto index = static_cast<size_t>(mapping.technique);
        const auto value = juce::jlimit(0.0f, 1.0f, mappedBreath);
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
    onsetTrace.record(message, timestampSeconds);
    const bool techniqueConsumed = handleTechniqueMessage(message, sink);
    bool releaseExistingMusicalNote = false;
    if (techniqueConsumed && message.isNoteOff())
    {
        const juce::SpinLock::ScopedLockType lock(noteMapLock);
        releaseExistingMusicalNote = activeOutputNotes[static_cast<size_t>(message.getNoteNumber())] >= 0;
    }
    if (techniqueConsumed && ! releaseExistingMusicalNote
        && !(message.isController() && message.getControllerNumber() == breathController.load()))
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
        ++activeNoteCount;
        latestNoteStartedMs.store(juce::Time::getMillisecondCounterHiRes(), std::memory_order_relaxed);
        intelligentTechniques.noteStarted(juce::Time::getMillisecondCounterHiRes());
        if (sink != nullptr) sink->noteOn(outputNote, safeVelocity, timestampSeconds);
    }
    else if (message.isNoteOff())
    {
        auto outputNote = juce::jlimit(0, 127, message.getNoteNumber() + transposeSemitones.load(std::memory_order_relaxed));
        { const juce::SpinLock::ScopedLockType lock(noteMapLock);
          const auto sourceNote = static_cast<size_t>(message.getNoteNumber());
          if (activeOutputNotes[sourceNote] >= 0) outputNote = activeOutputNotes[sourceNote];
          activeOutputNotes[sourceNote] = -1;
          if (lastNote.load(std::memory_order_relaxed) == outputNote)
          {
              int remainingNote = -1;
              for (const auto activeNote : activeOutputNotes)
                  if (activeNote >= 0) remainingNote = activeNote;
              lastNote.store(remainingNote, std::memory_order_relaxed);
          }
        }
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
        if (lock.isLocked() && detectingBreath.load(std::memory_order_acquire))
            controllerDetector.observe(message.getControllerNumber(), message.getControllerValue());
    }
}
void MidiInputService::tickTechniqueAutomation()
{
    auto* sink = performanceSink.load(std::memory_order_acquire);
    if (!sink) return;
    const juce::SpinLock::ScopedTryLockType lock(techniqueLock);
    if (!lock.isLocked()) return;
    const auto key = techniqueContext.toRawUTF8();
    for (const auto& mapping : techniqueMappings)
    {
        if (!simpleRoleSupported(key, mapping.technique)) continue;
        if (mapping.mode != TechniqueControlMode::breath && mapping.mode != TechniqueControlMode::automatic) continue;
        const auto b = breath.load(std::memory_order_relaxed);
        const bool playing = lastNote.load(std::memory_order_relaxed) >= 0 && b > 0.0f;
        if (mapping.technique == PerformanceTechnique::vibrato)
        {
            const auto elapsed = juce::Time::getMillisecondCounterHiRes() - latestNoteStartedMs.load(std::memory_order_relaxed);
            sink->automaticVibratoChanged(playing?b:0.0f,playing?MusicTempoPolicy::forMode(musicTempoMode.load()).fade(elapsed):0.0f);
        }
        else if (mapping.technique == PerformanceTechnique::growl)
            sink->techniqueChanged(mapping.technique, playing ? b : 0.0f);
    }
}
}
