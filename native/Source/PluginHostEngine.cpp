#include "PluginHostEngine.h"

#include <algorithm>
#include <limits>
#include <optional>

namespace fengyin
{
namespace
{
juce::String normalisedParameterName(juce::AudioProcessorParameter& parameter)
{
    return parameter.getName(160).toLowerCase().removeCharacters(" ._-/()[]");
}

bool isProtectedPerformanceParameter(const juce::String& name)
{
    // Tone styles must never rewrite the control layer used by smart wind-
    // controller adaptation or technique mapping.
    static const juce::StringArray protectedWords {
        "midi", "midicc", "controller", "mapping", "keyswitch", "channel",
        "breath", "expression", "aftertouch", "velocity", "dynamiccurve",
        "pitchbend", "bendrange", "growl", "vibrato", "flutter", "portamento",
        "falldown", "overblow", "halfvalve", "legato", "tremolo", "pizzicato",
        "bowpressure", "mutestate"
    };
    for (const auto& word : protectedWords)
        if (name.contains(word)) return true;
    return name.startsWith("cc") || name.endsWith("cc");
}

std::optional<float> acousticToneValue(const juce::String& name, const SwamToneProfile& profile)
{
    // SWAM's Breath Noise is an acoustic sound parameter, not the Breath /
    // Expression controller or its MIDI curve. Match only the exact exposed
    // sound-parameter names so controller mappings remain protected below.
    if (name == "breathnoise" || name == "breathnoiseamount") return profile.breathNoise;
    if (isProtectedPerformanceParameter(name)) return std::nullopt;
    if (name == "timbre" || name.contains("timbrecontrol")) return profile.timbre;
    if (name == "brightness" || name.contains("soundbrightness")) return profile.brightness;
    if (name.contains("formant")) return profile.formant;
    if (name.contains("reedstiffness") || name.contains("reedhardness")) return profile.reedStiffness;
    if (name == "attack" || name.contains("attacktime") || name.contains("attackshape")) return profile.attack;
    if (name.contains("harmonicstructure") || name == "harmonics" || name.contains("harmoniccontent")) return profile.harmonics;
    if (name.contains("keynoise") || name.contains("mechanicalnoise")) return profile.keyNoise;
    if (name == "resonance" || name.contains("boreresonance") || name.contains("bodyresonance")) return profile.resonance;
    return std::nullopt;
}
}

class TechniqueProcessingGraph final : public juce::AudioProcessorGraph
{
public:
    std::function<void()> applyTechniqueValues;
    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& messages) override
    {
        if (applyTechniqueValues) applyTechniqueValues();
        juce::AudioProcessorGraph::processBlock(buffer, messages);
    }
};
bool RecordingAudioProcessorPlayer::enqueueMidi(const juce::MidiMessage& message,
                                                double timestampSeconds) noexcept
{
    const auto size = message.getRawDataSize();
    if (size <= 0 || size > 3)
        return false;

    const auto timestamp = timestampSeconds > 0.0
        ? timestampSeconds : juce::Time::getMillisecondCounterHiRes() * 0.001;
    auto& queue = timestampSeconds > 0.0 ? midiQueue : controlQueue;
    if (! queue.push(message.getRawData(), size, timestamp))
    {
        resetRequested.store(true, std::memory_order_release);
        return false;
    }
    return true;
}

void RecordingAudioProcessorPlayer::addResetMessages(double timestampSeconds)
{
    auto& collector = getMidiMessageCollector();
    auto send = [&collector, timestampSeconds](juce::MidiMessage message)
    {
        message.setTimeStamp(timestampSeconds);
        collector.addMessageToQueue(message);
    };
    send(juce::MidiMessage::allNotesOff(1));
    send(juce::MidiMessage::allSoundOff(1));
    send(juce::MidiMessage::controllerEvent(1, 11, 0));
    send(juce::MidiMessage::controllerEvent(1, 2, 0));
    send(juce::MidiMessage::controllerEvent(1, 1, 0));
    send(juce::MidiMessage::pitchWheel(1, 8192));
}

void RecordingAudioProcessorPlayer::setLatencyProbeActive(bool active) noexcept
{
    latencyProbeActive.store(active, std::memory_order_release);
    if (! active)
        requestPerformanceReset();
}

void RecordingAudioProcessorPlayer::addLatencyProbeMessages(int numSamples, double timestampSeconds)
{
    probeSamplesUntilChange -= numSamples;
    auto& collector = getMidiMessageCollector();
    auto send = [&collector, timestampSeconds](juce::MidiMessage message)
    {
        message.setTimeStamp(timestampSeconds);
        collector.addMessageToQueue(message);
    };

    const auto phase = 1.0 - juce::jlimit(0.0, 1.0,
        static_cast<double>(juce::jmax(0, probeSamplesUntilChange)) / (currentSampleRate * 0.8));
    const auto breath = juce::jlimit(18, 105, 18 + juce::roundToInt(87.0 * phase));
    send(juce::MidiMessage::controllerEvent(1, 11, breath));
    send(juce::MidiMessage::controllerEvent(1, 2, breath));
    send(juce::MidiMessage::controllerEvent(1, 1, breath));

    if (probeSamplesUntilChange > 0)
        return;
    probeNoteIsOn = ! probeNoteIsOn;
    if (probeNoteIsOn)
    {
        send(juce::MidiMessage::noteOn(1, 67, static_cast<juce::uint8>(82)));
        probeSamplesUntilChange = juce::roundToInt(currentSampleRate * 0.8);
    }
    else
    {
        send(juce::MidiMessage::noteOff(1, 67));
        probeSamplesUntilChange = juce::roundToInt(currentSampleRate * 0.15);
    }
}

class PluginHostEngine::PluginEditorWindow final : public juce::DocumentWindow
{
public:
    PluginEditorWindow(const juce::String& title, std::unique_ptr<juce::AudioProcessorEditor> editor)
        : DocumentWindow(title, juce::Colour(0xff101d2c), closeButton)
    {
        const auto editorAllowsResize = editor->isResizable();
        setUsingNativeTitleBar(true);
        setContentOwned(editor.release(), true);
        // Respect fixed-size third-party editors; forcing them to relayout continuously
        // is a common source of sluggish VST3 windows on Windows.
        setResizable(editorAllowsResize, false);
        centreWithSize(juce::jmax(320, getWidth()), juce::jmax(240, getHeight()));
        setAlwaysOnTop(true);
        setVisible(true);
        toFront(true);
        grabKeyboardFocus();
    }
    void closeButtonPressed() override { setVisible(false); }
};

void RecordingAudioProcessorPlayer::audioDeviceIOCallbackWithContext(const float* const* inputs, int numInputs,
                                                                     float* const* outputs, int numOutputs,
                                                                     int numSamples,
                                                                     const juce::AudioIODeviceCallbackContext& context)
{
    const auto nowSeconds = juce::Time::getMillisecondCounterHiRes() * 0.001;
    if (resetRequested.exchange(false, std::memory_order_acq_rel))
        addResetMessages(nowSeconds);
    RealtimeMidiEvent event;
    for (std::size_t count = 0; count < midiQueueSize && midiQueue.pop(event); ++count)
    {
        juce::MidiMessage message(event.bytes.data(), static_cast<int>(event.size), event.timestampSeconds);
        getMidiMessageCollector().addMessageToQueue(message);
    }
    for (std::size_t count = 0; count < midiQueueSize && controlQueue.pop(event); ++count)
        getMidiMessageCollector().addMessageToQueue(
            juce::MidiMessage(event.bytes.data(), static_cast<int>(event.size), event.timestampSeconds));
    const auto probing = latencyProbeActive.load(std::memory_order_acquire);
    if (probing)
        addLatencyProbeMessages(numSamples, nowSeconds);

    juce::AudioProcessorPlayer::audioDeviceIOCallbackWithContext(inputs, numInputs, outputs, numOutputs, numSamples, context);
    bool signal = false;
    for (int channel = 0; channel < numOutputs; ++channel)
        if (outputs[channel] != nullptr)
            for (int i = 0; i < numSamples && ! signal; ++i)
                signal = std::abs(outputs[channel][i]) > 0.00001f;
    if (signal) signalBlocks.fetch_add(1, std::memory_order_relaxed);
    if (masterOutput != nullptr)
        masterOutput->processInstrument(outputs, numOutputs, numSamples);
    if (accompaniment != nullptr)
        accompaniment->mixInto(outputs, numOutputs, numSamples);
    if (masterOutput != nullptr)
        masterOutput->processMaster(outputs, numOutputs, numSamples);
    if (probing)
        for (int channel = 0; channel < numOutputs; ++channel)
            if (outputs[channel] != nullptr)
                juce::FloatVectorOperations::clear(outputs[channel], numSamples);
    if (recorder != nullptr)
        recorder->push(outputs, numOutputs, numSamples);
    callbackCount.fetch_add(1, std::memory_order_relaxed);
    if (juce::Time::getMillisecondCounterHiRes() * 0.001 - nowSeconds
        > static_cast<double>(numSamples) / currentSampleRate)
        callbackOverruns.fetch_add(1, std::memory_order_relaxed);
}

void RecordingAudioProcessorPlayer::audioDeviceAboutToStart(juce::AudioIODevice* device)
{
    juce::AudioProcessorPlayer::audioDeviceAboutToStart(device);
    getMidiMessageCollector().ensureStorageAllocated(256 * 1024);
    currentSampleRate = device != nullptr && device->getCurrentSampleRate() > 0.0
        ? device->getCurrentSampleRate() : 48000.0;
    probeSamplesUntilChange = 0;
    probeNoteIsOn = false;
    if (accompaniment != nullptr && device != nullptr)
        accompaniment->prepare(device->getCurrentSampleRate(), device->getCurrentBufferSizeSamples());
    if (masterOutput != nullptr && device != nullptr)
        masterOutput->setSampleRate(device->getCurrentSampleRate());
}

void RecordingAudioProcessorPlayer::audioDeviceStopped()
{
    if (accompaniment != nullptr)
        accompaniment->release();
    juce::AudioProcessorPlayer::audioDeviceStopped();
}

PluginHostEngine::PluginHostEngine()
{
    juce::addDefaultFormatsToManager(formatManager);
}

PluginHostEngine::~PluginHostEngine()
{
    lifetime->store(false, std::memory_order_release);
    detach();
    unload();
}

void PluginHostEngine::attachTo(juce::AudioDeviceManager& deviceManager)
{
    if (attachedManager == &deviceManager)
        return;
    detach();
    attachedManager = &deviceManager;
    attachedManager->addAudioCallback(&player);
}

void PluginHostEngine::detach()
{
    if (attachedManager != nullptr)
        attachedManager->removeAudioCallback(&player);
    attachedManager = nullptr;
}

void PluginHostEngine::loadAsync(const juce::PluginDescription& description,
                                 double sampleRate,
                                 int bufferSize,
                                 LoadCallback callback)
{
    unload();
    formatManager.createPluginInstanceAsync(
        description, sampleRate, bufferSize,
        [this, description, sampleRate, bufferSize, guard = lifetime, completion = std::move(callback)]
        (std::unique_ptr<juce::AudioPluginInstance> instance, const juce::String& error) mutable
        {
            if (! guard->load(std::memory_order_acquire))
                return;
            if (instance == nullptr)
            {
                if (completion)
                    completion(false, error.isNotEmpty() ? error : juce::String("VST3 load failed"));
                return;
            }
            auto processingGraph = std::make_unique<TechniqueProcessingGraph>();
            processingGraph->applyTechniqueValues = [this] { flushTechniqueValues(); };
            graph = std::move(processingGraph);
            // Configure the graph's output buses before adding the IO node. Without this,
            // the output node initially has zero input channels and silently rejects every
            // instrument-to-output connection while accompaniment audio still remains audible.
            graph->setPlayConfigDetails(0, 2, sampleRate, bufferSize);
            audioOutputNode = graph->addNode(std::make_unique<juce::AudioProcessorGraph::AudioGraphIOProcessor>(
                juce::AudioProcessorGraph::AudioGraphIOProcessor::audioOutputNode));
            midiInputNode = graph->addNode(std::make_unique<juce::AudioProcessorGraph::AudioGraphIOProcessor>(
                juce::AudioProcessorGraph::AudioGraphIOProcessor::midiInputNode));
            instrumentNode = graph->addNode(std::move(instance));
            currentDescription = description;
            resolveTechniqueParameters();
            resolveInstrumentModelParameter();
            if (! rebuildConnections())
            {
                unload();
                if (completion)
                    completion(false, juce::String::fromUTF8("音源已打开，但无法连接到声音输出，请重新选择声音设备"));
                return;
            }
            player.setProcessor(graph.get());
            resetPerformance();
            if (completion)
                completion(true, instrumentNode->getProcessor()->getName());
        });
}

void PluginHostEngine::unload()
{
    instrumentEditorWindow.reset();
    effectEditorWindow.reset();
    player.setProcessor(nullptr);
    effectNode = nullptr;
    instrumentNode = nullptr;
    audioOutputNode = nullptr;
    midiInputNode = nullptr;
    graph.reset();
    currentDescription = {};
    currentEffectDescription = {};
    for (auto& parameter : techniqueParameters) parameter.store(nullptr);
    for (auto& dirty : techniqueDirty) dirty.store(false, std::memory_order_relaxed);
    instrumentModelParameter = nullptr;
    instrumentModelNames.clear();
}

bool PluginHostEngine::hasPlugin() const noexcept
{
    return instrumentNode != nullptr;
}

juce::String PluginHostEngine::getPluginName() const
{
    return instrumentNode != nullptr ? instrumentNode->getProcessor()->getName() : juce::String();
}

juce::String PluginHostEngine::getPluginIdentifier() const
{
    return instrumentNode != nullptr ? currentDescription.createIdentifierString() : juce::String();
}

juce::AudioPluginInstance* PluginHostEngine::getPlugin() const noexcept
{
    return instrumentNode != nullptr ? dynamic_cast<juce::AudioPluginInstance*>(instrumentNode->getProcessor()) : nullptr;
}

juce::String PluginHostEngine::getEffectName() const
{
    return effectNode != nullptr ? effectNode->getProcessor()->getName() : juce::String();
}

juce::String PluginHostEngine::getEffectIdentifier() const
{
    return effectNode != nullptr ? currentEffectDescription.createIdentifierString() : juce::String();
}

juce::MemoryBlock PluginHostEngine::saveEffectState() const
{
    juce::MemoryBlock state;
    if (effectNode != nullptr)
        effectNode->getProcessor()->getStateInformation(state);
    return state;
}

bool PluginHostEngine::restoreEffectState(const void* data, std::size_t size)
{
    if (effectNode == nullptr || data == nullptr || size == 0 || size > static_cast<std::size_t>(std::numeric_limits<int>::max()))
        return false;
    effectNode->getProcessor()->setStateInformation(data, static_cast<int>(size));
    return true;
}

juce::MemoryBlock PluginHostEngine::savePluginState() const
{
    juce::MemoryBlock state;
    if (auto* plugin = getPlugin())
        plugin->getStateInformation(state);
    return state;
}

bool PluginHostEngine::restorePluginState(const void* data, std::size_t size)
{
    auto* plugin = getPlugin();
    if (plugin == nullptr || data == nullptr || size == 0 || size > static_cast<std::size_t>(std::numeric_limits<int>::max()))
        return false;
    plugin->setStateInformation(data, static_cast<int>(size));
    return true;
}

void PluginHostEngine::loadEffectAsync(const juce::PluginDescription& description, double sampleRate,
                                       int bufferSize, LoadCallback callback)
{
    if (graph == nullptr || instrumentNode == nullptr)
    {
        if (callback) callback(false, juce::String::fromUTF8("请先加载音源"));
        return;
    }
    formatManager.createPluginInstanceAsync(description, sampleRate, bufferSize,
        [this, description, guard = lifetime, completion = std::move(callback)](std::unique_ptr<juce::AudioPluginInstance> instance,
                                                                                const juce::String& error) mutable
        {
            if (! guard->load(std::memory_order_acquire)) return;
            if (instance == nullptr)
            {
                if (completion) completion(false, error.isNotEmpty() ? error : juce::String::fromUTF8("效果器加载失败"));
                return;
            }
            player.setProcessor(nullptr);
            effectEditorWindow.reset();
            if (effectNode != nullptr) graph->removeNode(effectNode->nodeID);
            effectNode = graph->addNode(std::move(instance));
            currentEffectDescription = description;
            if (! rebuildConnections())
            {
                graph->removeNode(effectNode->nodeID);
                effectNode = nullptr;
                currentEffectDescription = {};
                rebuildConnections();
                player.setProcessor(graph.get());
                if (completion) completion(false, juce::String::fromUTF8("效果器音频通道不兼容，已恢复直接输出"));
                return;
            }
            player.setProcessor(graph.get());
            if (completion) completion(true, effectNode->getProcessor()->getName());
        });
}

void PluginHostEngine::unloadEffect()
{
    if (graph == nullptr || effectNode == nullptr) return;
    player.setProcessor(nullptr);
    effectEditorWindow.reset();
    graph->removeNode(effectNode->nodeID);
    effectNode = nullptr;
    currentEffectDescription = {};
    rebuildConnections();
    player.setProcessor(graph.get());
}

bool PluginHostEngine::setEffectBypassed(bool shouldBypass)
{
    if (effectNode == nullptr) return false;
    effectNode->setBypassed(shouldBypass);
    return true;
}

bool PluginHostEngine::isEffectBypassed() const noexcept
{
    return effectNode != nullptr && effectNode->isBypassed();
}

bool PluginHostEngine::showPluginEditor(bool effect)
{
    auto node = effect ? effectNode : instrumentNode;
    if (node == nullptr) return false;
    auto& window = effect ? effectEditorWindow : instrumentEditorWindow;
    if (window != nullptr)
    {
        window->setVisible(true);
        window->toFront(true);
        return true;
    }
    auto* processor = node->getProcessor();
    std::unique_ptr<juce::AudioProcessorEditor> editor(processor->createEditorAndMakeActive());
    if (editor == nullptr)
        editor = std::make_unique<juce::GenericAudioProcessorEditor>(*processor);
    window = std::make_unique<PluginEditorWindow>((effect ? juce::String::fromUTF8("效果器 · ")
                                                         : juce::String::fromUTF8("音源 · ")) + processor->getName(),
                                                   std::move(editor));
    return true;
}

bool PluginHostEngine::rebuildConnections()
{
    if (graph == nullptr || instrumentNode == nullptr || audioOutputNode == nullptr || midiInputNode == nullptr) return false;
    for (const auto& connection : graph->getConnections())
        graph->removeConnection(connection);
    const auto midiConnected = graph->addConnection(
        { { midiInputNode->nodeID, juce::AudioProcessorGraph::midiChannelIndex },
          { instrumentNode->nodeID, juce::AudioProcessorGraph::midiChannelIndex } });
    auto source = instrumentNode;
    if (effectNode != nullptr)
    {
        const auto sourceChannels = juce::jmax(1, instrumentNode->getProcessor()->getTotalNumOutputChannels());
        const auto destinationChannels = juce::jmax(1, effectNode->getProcessor()->getTotalNumInputChannels());
        int effectConnections = 0;
        for (int channel = 0; channel < juce::jmin(2, destinationChannels); ++channel)
            if (graph->addConnection({ { instrumentNode->nodeID, juce::jmin(channel, sourceChannels - 1) },
                                        { effectNode->nodeID, channel } }))
                ++effectConnections;
        if (effectConnections == 0) return false;
        source = effectNode;
    }
    const auto sourceChannels = juce::jmax(1, source->getProcessor()->getTotalNumOutputChannels());
    int audioConnections = 0;
    for (int channel = 0; channel < 2; ++channel)
        if (graph->addConnection({ { source->nodeID, juce::jmin(channel, sourceChannels - 1) },
                                    { audioOutputNode->nodeID, channel } }))
            ++audioConnections;
    return midiConnected && audioConnections > 0;
}

void PluginHostEngine::noteOn(int noteNumber, float velocity, double timestampSeconds) noexcept
{
    queue(juce::MidiMessage::noteOn(1, juce::jlimit(0, 127, noteNumber),
                                    juce::jlimit(0.0f, 1.0f, velocity)), timestampSeconds);
}

void PluginHostEngine::noteOff(int noteNumber, double timestampSeconds) noexcept
{
    queue(juce::MidiMessage::noteOff(1, juce::jlimit(0, 127, noteNumber)), timestampSeconds);
}

void PluginHostEngine::breathChanged(float value, double timestampSeconds) noexcept
{
    const auto midiValue = juce::jlimit(0, 127, juce::roundToInt(value * 127.0f));
    if (lastBreathMidiValue.exchange(midiValue, std::memory_order_relaxed) == midiValue)
        return;
    queue(juce::MidiMessage::controllerEvent(1, 11, midiValue), timestampSeconds);
    queue(juce::MidiMessage::controllerEvent(1, 2, midiValue), timestampSeconds);
    // Qin Engine soundbanks commonly use the modulation wheel for expression.
    // Keep CC11 as well so user-edited programs remain playable.
    if (kongExpressionMode.load(std::memory_order_relaxed))
        queue(juce::MidiMessage::controllerEvent(1, 1, midiValue), timestampSeconds);
}

void PluginHostEngine::pitchBendChanged(float bipolarValue, double timestampSeconds) noexcept
{
    const auto pitch = juce::jlimit(0, 16383,
        juce::roundToInt(8192.0f + juce::jlimit(-1.0f, 1.0f, bipolarValue)
                         * (bipolarValue < 0.0f ? 8192.0f : 8191.0f)));
    queue(juce::MidiMessage::pitchWheel(1, pitch), timestampSeconds);
}

void PluginHostEngine::techniqueChanged(PerformanceTechnique technique, float value) noexcept
{
    const auto index = static_cast<size_t>(technique);
    if (index >= techniqueValues.size()) return;
    techniqueValues[index].store(juce::jlimit(0.0f, 1.0f, value), std::memory_order_relaxed);
    techniqueDirty[index].store(true, std::memory_order_release);
}

void PluginHostEngine::resetPerformance() noexcept
{
    lastBreathMidiValue.store(-1, std::memory_order_relaxed);
    player.requestPerformanceReset();
    for (size_t index = 0; index < techniqueValues.size(); ++index)
    {
        techniqueValues[index].store(0.0f, std::memory_order_relaxed);
        techniqueDirty[index].store(true, std::memory_order_release);
    }
}

juce::StringArray PluginHostEngine::getProgramNames() const
{
    juce::StringArray names;
    if (auto* processor = getPlugin())
        for (int index = 0; index < processor->getNumPrograms(); ++index)
            names.add(processor->getProgramName(index));
    return names;
}

juce::String PluginHostEngine::getCurrentProgramName() const
{
    if (auto* processor = getPlugin())
        return processor->getProgramName(processor->getCurrentProgram());
    return {};
}

bool PluginHostEngine::selectProgramByAliases(const juce::StringArray& aliases)
{
    auto* processor = getPlugin();
    if (processor == nullptr) return false;
    for (int index = 0; index < processor->getNumPrograms(); ++index)
    {
        const auto candidate = processor->getProgramName(index).toLowerCase().removeCharacters(" ._-");
        for (const auto& alias : aliases)
            if (candidate.contains(alias.toLowerCase().removeCharacters(" ._-")))
            {
                processor->setCurrentProgram(index);
                return true;
            }
    }
    return false;
}

juce::StringArray PluginHostEngine::getInstrumentModelNames() const
{
    return instrumentModelNames;
}

int PluginHostEngine::getCurrentInstrumentModelIndex() const noexcept
{
    if (instrumentModelParameter == nullptr || instrumentModelNames.size() < 2) return -1;
    return juce::jlimit(0, instrumentModelNames.size() - 1,
        juce::roundToInt(instrumentModelParameter->getValue() * static_cast<float>(instrumentModelNames.size() - 1)));
}

juce::String PluginHostEngine::getCurrentInstrumentModelName() const
{
    const auto index = getCurrentInstrumentModelIndex();
    return juce::isPositiveAndBelow(index, instrumentModelNames.size()) ? instrumentModelNames[index] : juce::String();
}

bool PluginHostEngine::selectInstrumentModel(int index)
{
    if (instrumentModelParameter == nullptr || ! juce::isPositiveAndBelow(index, instrumentModelNames.size()))
        return false;
    const auto value = static_cast<float>(index) / static_cast<float>(instrumentModelNames.size() - 1);
    instrumentModelParameter->beginChangeGesture();
    instrumentModelParameter->setValueNotifyingHost(value);
    instrumentModelParameter->endChangeGesture();
    return true;
}

int PluginHostEngine::applySwamToneProfile(const SwamToneProfile& profile)
{
    auto* plugin = getPlugin();
    if (plugin == nullptr || ! profile.enabled) return 0;
    int changed = 0;
    for (auto* parameter : plugin->getParameters())
    {
        if (parameter == nullptr || parameter == instrumentModelParameter) continue;
        const auto target = acousticToneValue(normalisedParameterName(*parameter), profile);
        if (! target.has_value()) continue;
        parameter->beginChangeGesture();
        parameter->setValueNotifyingHost(juce::jlimit(0.0f, 1.0f, *target));
        parameter->endChangeGesture();
        ++changed;
    }
    return changed;
}

void PluginHostEngine::flushTechniqueValues()
{
    for (size_t index = 0; index < techniqueParameters.size(); ++index)
        if (techniqueDirty[index].exchange(false, std::memory_order_acq_rel))
            if (auto* parameter = techniqueParameters[index].load(std::memory_order_acquire))
                parameter->setValueNotifyingHost(techniqueValues[index].load(std::memory_order_relaxed));
}

bool PluginHostEngine::supportsTechnique(PerformanceTechnique technique) const noexcept
{
    const auto index = static_cast<size_t>(technique);
    return index < techniqueParameters.size() && techniqueParameters[index] != nullptr;
}

int PluginHostEngine::getProcessingLatencySamples() const noexcept
{
    int samples = instrumentNode != nullptr ? instrumentNode->getProcessor()->getLatencySamples() : 0;
    if (effectNode != nullptr && ! effectNode->isBypassed())
        samples += effectNode->getProcessor()->getLatencySamples();
    return juce::jmax(0, samples);
}

void PluginHostEngine::resolveTechniqueParameters()
{
    for (auto& parameter : techniqueParameters) parameter.store(nullptr);
    if (instrumentNode == nullptr) return;
    for (auto* parameter : instrumentNode->getProcessor()->getParameters())
    {
        const auto name = parameter->getName(128).toLowerCase().removeCharacters(" ._-");
        if (techniqueParameters[static_cast<size_t>(PerformanceTechnique::growl)] == nullptr
            && name.contains("growl"))
            techniqueParameters[static_cast<size_t>(PerformanceTechnique::growl)] = parameter;
        else if (techniqueParameters[static_cast<size_t>(PerformanceTechnique::vibrato)] == nullptr
                 && (name.contains("vibratodepth") || name.contains("vibrdepth")))
            techniqueParameters[static_cast<size_t>(PerformanceTechnique::vibrato)] = parameter;
        else if (techniqueParameters[static_cast<size_t>(PerformanceTechnique::flutter)] == nullptr
                 && name.contains("flutter"))
            techniqueParameters[static_cast<size_t>(PerformanceTechnique::flutter)] = parameter;
        else if (techniqueParameters[static_cast<size_t>(PerformanceTechnique::portamento)] == nullptr
                 && name.contains("portamento") && ! name.contains("split"))
            techniqueParameters[static_cast<size_t>(PerformanceTechnique::portamento)] = parameter;
        else if (techniqueParameters[static_cast<size_t>(PerformanceTechnique::fall)] == nullptr
                 && (name.contains("falldown") || name == "fall" || name.contains("doit")))
            techniqueParameters[static_cast<size_t>(PerformanceTechnique::fall)] = parameter;
        else if (techniqueParameters[static_cast<size_t>(PerformanceTechnique::overblow)] == nullptr
                 && name.contains("overblow"))
            techniqueParameters[static_cast<size_t>(PerformanceTechnique::overblow)] = parameter;
        else if (techniqueParameters[static_cast<size_t>(PerformanceTechnique::breathNoise)] == nullptr
                 && name.contains("breathnoise"))
            techniqueParameters[static_cast<size_t>(PerformanceTechnique::breathNoise)] = parameter;
        else if (techniqueParameters[static_cast<size_t>(PerformanceTechnique::alternateFingering)] == nullptr
                 && (name.contains("altfingering") || name.contains("alternatefingering")))
            techniqueParameters[static_cast<size_t>(PerformanceTechnique::alternateFingering)] = parameter;
        else if (techniqueParameters[static_cast<size_t>(PerformanceTechnique::mute)] == nullptr
                 && (name == "mute" || name.contains("mutestate") || name.contains("handmute")))
            techniqueParameters[static_cast<size_t>(PerformanceTechnique::mute)] = parameter;
        else if (techniqueParameters[static_cast<size_t>(PerformanceTechnique::halfValve)] == nullptr
                 && name.contains("halfvalve"))
            techniqueParameters[static_cast<size_t>(PerformanceTechnique::halfValve)] = parameter;
        else if (techniqueParameters[static_cast<size_t>(PerformanceTechnique::legato)] == nullptr
                 && (name == "legato" || name.contains("legatomode")))
            techniqueParameters[static_cast<size_t>(PerformanceTechnique::legato)] = parameter;
        else if (techniqueParameters[static_cast<size_t>(PerformanceTechnique::bowPressure)] == nullptr
                 && name.contains("bowpressure"))
            techniqueParameters[static_cast<size_t>(PerformanceTechnique::bowPressure)] = parameter;
        else if (techniqueParameters[static_cast<size_t>(PerformanceTechnique::pizzicato)] == nullptr
                 && (name.contains("pizzicato") || name == "pizz"))
            techniqueParameters[static_cast<size_t>(PerformanceTechnique::pizzicato)] = parameter;
        else if (techniqueParameters[static_cast<size_t>(PerformanceTechnique::tremolo)] == nullptr
                 && name.contains("tremolo"))
            techniqueParameters[static_cast<size_t>(PerformanceTechnique::tremolo)] = parameter;
    }
}

void PluginHostEngine::resolveInstrumentModelParameter()
{
    instrumentModelParameter = nullptr;
    instrumentModelNames.clear();
    if (instrumentNode == nullptr) return;
    int bestScore = 0;
    for (auto* parameter : instrumentNode->getProcessor()->getParameters())
    {
        if (parameter == nullptr) continue;
        const auto name = normalisedParameterName(*parameter);
        if (isProtectedPerformanceParameter(name)) continue;
        int score = 0;
        if (name == "instrumentmodel" || name == "saxmodel") score = 120;
        else if (name == "instrument" || name == "model") score = 100;
        else if (name.contains("instrumentmodel") || name.contains("saxmodel")) score = 90;
        else if (name == "bodymodel") score = 80;
        if (score <= bestScore) continue;
        const auto steps = parameter->getNumSteps();
        if (steps < 2 || steps > 64) continue;
        juce::StringArray names;
        for (int index = 0; index < steps; ++index)
        {
            const auto value = static_cast<float>(index) / static_cast<float>(steps - 1);
            auto label = parameter->getText(value, 96).trim();
            if (label.isEmpty()) label = juce::String::fromUTF8("型号 ") + juce::String(index + 1);
            names.add(label);
        }
        // Keep the array step-aligned. Removing duplicate labels would change the
        // normalised value used for later entries and could select the wrong model.
        auto distinctNames = names;
        distinctNames.removeDuplicates(false);
        if (distinctNames.size() < 2) continue;
        bestScore = score;
        instrumentModelParameter = parameter;
        instrumentModelNames = names;
    }
}

void PluginHostEngine::queue(juce::MidiMessage message, double timestampSeconds) noexcept
{
    (void) player.enqueueMidi(message, timestampSeconds);
}
}
