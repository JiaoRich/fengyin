#include "PluginHostEngine.h"
#include "BreathResponse.h"
#include "SerialEffectRouting.h"
#include "ScopedGraphPause.h"
#include "KongInstrumentCatalog.h"
#include "KongProjectFile.h"
#include "KongTechniqueMap.h"
#include "KongPerformancePolicy.h"
#include "BendRangeParameters.h"
#include "PerformanceReset.h"
#include "ToneRestorePlan.h"

#include <algorithm>
#include <cmath>
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

bool isMidiRoutingParameter(const juce::String& name)
{
    // A custom tone may legitimately include growl, vibrato, breath-noise and
    // expression-curve parameters.  Only controller routing is owned by the
    // smart wind-controller layer and must never travel with a tone preset.
    static const juce::StringArray routingWords {
        "midi", "midicc", "controller", "mapping", "keyswitch", "channel",
        "ccnumber", "ccassign", "ccassignment", "learncc"
    };
    for (const auto& word : routingWords)
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

juce::String stableParameterIdentifier(juce::AudioProcessorParameter& parameter, int index)
{
    if (auto* identified = dynamic_cast<juce::AudioProcessorParameterWithID*>(&parameter))
        return "id:" + identified->paramID;
    return "index:" + juce::String(index) + ":" + normalisedParameterName(parameter);
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
    if (! midiQueue.push(message.getRawData(), size, timestamp))
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
    sendPerformanceReset(kongResetMode.load(std::memory_order_acquire), send);
}

void RecordingAudioProcessorPlayer::setLatencyProbeActive(bool active) noexcept
{
    latencyProbeActive.store(active, std::memory_order_release);
    if (active)
    {
        probeSamplesUntilChange = 0;
        probeNoteIsOn = false;
        probeNoteIndex = 0;
        probeCurrentNote = 67;
    }
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
        // Cover low, middle and high mappings. Container sample players may
        // expose a narrow playable range or map percussion to only part of the
        // keyboard, so probing only G4 can incorrectly report silence.
        static constexpr std::array<int, 6> probeNotes { 48, 55, 60, 67, 72, 79 };
        probeCurrentNote = probeNotes[static_cast<size_t>(probeNoteIndex++ % static_cast<int>(probeNotes.size()))];
        send(juce::MidiMessage::noteOn(1, probeCurrentNote, static_cast<juce::uint8>(82)));
        probeSamplesUntilChange = juce::roundToInt(currentSampleRate * 0.8);
    }
    else
    {
        send(juce::MidiMessage::noteOff(1, probeCurrentNote));
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
        onsetTrace.record(message, event.timestampSeconds);
        getMidiMessageCollector().addMessageToQueue(message);
    }
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
    float peak = 0;
    for (int channel = 0; channel < numOutputs; ++channel)
        if (outputs[channel] != nullptr)
            for (int i = 0; i < numSamples; ++i)
                peak = std::max(peak, std::abs(outputs[channel][i]));
    auto previousPeak = diagnosticSourcePeak.load(std::memory_order_relaxed);
    while (previousPeak < peak && ! diagnosticSourcePeak.compare_exchange_weak(previousPeak, peak)) {}
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
    probeNoteIndex = 0;
    probeCurrentNote = 67;
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
    loadAsync(description, sampleRate, bufferSize, {}, std::move(callback));
}

void PluginHostEngine::loadAsync(const juce::PluginDescription& description,
                                 double sampleRate,
                                 int bufferSize,
                                 const juce::MemoryBlock& initialState,
                                 LoadCallback callback)
{
    unload();
    const auto generation = loadGeneration;
    juce::Logger::writeToLog("Plugin load request " + juce::String(generation) + ": " + description.name);
    formatManager.createPluginInstanceAsync(
        description, sampleRate, bufferSize,
        [this, description, sampleRate, bufferSize, initialState, generation,
         guard = lifetime, completion = std::move(callback)]
        (std::unique_ptr<juce::AudioPluginInstance> instance, const juce::String& error) mutable
        {
            if (! guard->load(std::memory_order_acquire))
                return;
            if (generation != loadGeneration) return;
            juce::Logger::writeToLog("Plugin instance ready " + juce::String(generation));
            if (instance == nullptr)
            {
                if (completion)
                    completion(false, error.isNotEmpty() ? error : juce::String("VST3 load failed"));
                return;
            }
            // Restore the original plugin-produced VST state, never a synthetic
            // translation of the manufacturer's separate project-file format.
            if (initialState.getSize() > 0)
            {
                if (initialState.getSize() > 64 * 1024 * 1024)
                {
                    if (completion)
                        completion(false, juce::String::fromUTF8("音源状态文件异常"));
                    return;
                }
                juce::Logger::writeToLog("Plugin state restore begin; bytes=" + juce::String(initialState.getSize()));
                instance->setStateInformation(initialState.getData(),
                                              static_cast<int>(initialState.getSize()));
                juce::Logger::writeToLog("Plugin state restore returned");
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
            releaseBaseline = captureToneParameters();
            previousReleaseParameters.clear();
            if (! rebuildConnections())
            {
                unload();
                if (completion)
                    completion(false, juce::String::fromUTF8("音源已打开，但无法连接到声音输出，请重新选择声音设备"));
                return;
            }
            // Set the reset policy before exposing the new processor to an
            // already-running device callback (including SWAM -> Qin switches).
            setKongExpressionMode(fengyin::SupportedInstrumentClassifier::classify(
                description.name, description.manufacturerName, description.fileOrIdentifier)
                    == fengyin::SupportedInstrumentClassifier::Brand::kong);
            // Keep the entire caller's initialization transaction silent, not
            // just individual parameter writes. The device may already be live.
            graph->suspendProcessing(true);
            player.setProcessor(graph.get());
            juce::Logger::writeToLog("Plugin graph ready " + juce::String(generation));
            resetPerformance();
            juce::Logger::writeToLog("Plugin performance reset queued " + juce::String(generation));
            if (completion)
                completion(true, instrumentNode->getProcessor()->getName());
            // A completion may replace/unload the graph. Never retain a raw
            // graph pointer across that callback.
            if (guard->load(std::memory_order_acquire) && generation == loadGeneration && graph)
                graph->suspendProcessing(false);
        });
}

void PluginHostEngine::unload()
{
    juce::Logger::writeToLog("Plugin unload begin");
    ++loadGeneration;
    ++effectLoadGeneration;
    player.setProcessor(nullptr);
    // Stop audio before destroying third-party editors and their processors.
    instrumentEditorWindow.reset();
    effectEditorWindow.reset();
    effectNodes.clear();
    instrumentNode = nullptr;
    audioOutputNode = nullptr;
    midiInputNode = nullptr;
    graph.reset();
    currentDescription = {};
    for (auto& parameter : techniqueParameters) parameter.store(nullptr);
    for (auto& dirty : techniqueDirty) dirty.store(false, std::memory_order_relaxed);
    instrumentModelParameter = nullptr;
    instrumentModelNames.clear();
    instrumentModelValues.clear();
    swamExpressionController.store(11, std::memory_order_relaxed);
    juce::Logger::writeToLog("Plugin unload complete");
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
    return hasEffect() ? effectNodes.front().node->getProcessor()->getName() : juce::String();
}

juce::String PluginHostEngine::getEffectIdentifier() const
{
    return hasEffect() ? effectNodes.front().description.createIdentifierString() : juce::String();
}

juce::MemoryBlock PluginHostEngine::saveEffectState() const
{
    juce::MemoryBlock state;
    if (hasEffect())
        effectNodes.front().node->getProcessor()->getStateInformation(state);
    return state;
}

bool PluginHostEngine::restoreEffectState(const void* data, std::size_t size)
{
    if (! hasEffect() || data == nullptr || size == 0 || size > static_cast<std::size_t>(std::numeric_limits<int>::max()))
        return false;
    const ScopedGraphPause pause(graph.get());
    effectNodes.front().node->getProcessor()->setStateInformation(data, static_cast<int>(size));
    // Resume without releasing the existing instrument.
    return true;
}

juce::Array<ToneParameterValue> PluginHostEngine::captureToneParameters() const
{
    juce::Array<ToneParameterValue> result;
    auto* plugin = getPlugin();
    if (plugin == nullptr) return result;
    const auto& parameters = plugin->getParameters();
    for (int index = 0; index < parameters.size(); ++index)
    {
        auto* parameter = parameters[index];
        if (parameter == nullptr || parameter == instrumentModelParameter) continue;
        if (isMidiRoutingParameter(normalisedParameterName(*parameter))) continue;
        result.add({ stableParameterIdentifier(*parameter, index), parameter->getValue() });
    }
    return result;
}

int PluginHostEngine::restoreToneParameters(const juce::Array<ToneParameterValue>& stored)
{
    const ScopedGraphPause pause(graph.get());
    juce::Logger::writeToLog("Tone parameter restore begin; count=" + juce::String(stored.size()));
    auto* plugin = getPlugin();
    if (plugin == nullptr) return 0;
    int restored = 0;
    const auto& parameters = plugin->getParameters();
    for (int index = 0; index < parameters.size(); ++index)
    {
        auto* parameter = parameters[index];
        if (parameter == nullptr || parameter == instrumentModelParameter) continue;
        if (isMidiRoutingParameter(normalisedParameterName(*parameter))) continue;
        const auto identifier = stableParameterIdentifier(*parameter, index);
        for (const auto& value : stored)
            if (value.identifier == identifier)
            {
                if (toneValueNeedsWrite(parameter->getValue(), value.value))
                {
                    juce::Logger::writeToLog("Tone write begin: " + identifier + "=" + juce::String(value.value, 7));
                    parameter->beginChangeGesture();
                    parameter->setValueNotifyingHost(value.value);
                    parameter->endChangeGesture();
                    juce::Logger::writeToLog("Tone write returned: " + identifier);
                }
                ++restored;
                break;
            }
    }
    juce::Logger::writeToLog("Tone parameter restore complete; count=" + juce::String(restored));
    return restored;
}

bool PluginHostEngine::applyExpressionAttackControl()
{
    const ScopedGraphPause pause(graph.get());
    auto* plugin = getPlugin();
    if (plugin == nullptr) return false;
    for (auto* parameter : plugin->getParameters())
    {
        if (parameter == nullptr || normalisedParameterName(*parameter) != "attackcontrol") continue;
        const auto previous = parameter->getValue();
        if (parameter->getText(previous, 128).trim().equalsIgnoreCase("Expression")) return true;
        // Resolve the displayed enum, not a saxophone-specific parameter index.
        for (int step = 0; step <= 1000; ++step)
        {
            const auto value = static_cast<float>(step) / 1000.0f;
            if (! parameter->getText(value, 128).trim().equalsIgnoreCase("Expression")) continue;
            parameter->beginChangeGesture();
            parameter->setValueNotifyingHost(value);
            parameter->endChangeGesture();
            const auto verified = parameter->getText(parameter->getValue(), 128).trim().equalsIgnoreCase("Expression");
            if (! verified)
            {
                parameter->beginChangeGesture();
                parameter->setValueNotifyingHost(previous);
                parameter->endChangeGesture();
            }
            juce::Logger::writeToLog(verified ? "SWAM Attack Control verified: Expression"
                                            : "SWAM Attack Control verification failed");
            return verified;
        }
    }
    juce::Logger::writeToLog("SWAM Attack Control unavailable: no verified Expression option; unchanged");
    return false;
}

bool PluginHostEngine::applyStandardSwamExpressionCurve()
{
    // The player may still be attached after replacing an instrument. Never
    // restore opaque plugin state concurrently with its audio callback.
    const ScopedGraphPause pause(graph.get());
    juce::Logger::writeToLog("SWAM expression state update begin");
    auto* plugin = getPlugin();
    if (plugin == nullptr) return false;

    juce::MemoryBlock state;
    plugin->getStateInformation(state);
    const auto result = SwamExpressionCurve::applyToState(state);
    if (! result.found)
    {
        juce::Logger::writeToLog("SWAM expression state update skipped: no supported curve");
        return false;
    }

    // Preserve and follow the controller already selected inside SWAM. The
    // smart adapter translates any supported wind controller to this one
    // destination, so changing the curve never rewrites the user's mapping.
    if (result.controller >= 0 && result.controller <= 127)
        swamExpressionController.store(result.controller, std::memory_order_relaxed);
    if (result.changed)
    {
        plugin->setStateInformation(state.getData(), static_cast<int>(state.getSize()));
        resolveTechniqueParameters();
        resolveInstrumentModelParameter();
        resetPerformance();
    }
    juce::Logger::writeToLog("SWAM expression state update complete");
    return true;
}

void PluginHostEngine::loadEffectAsync(const juce::PluginDescription& description, double sampleRate,
                                       int bufferSize, LoadCallback callback)
{
    if (effectNodes.size() >= 16)
    {
        if (callback) callback(false, juce::String::fromUTF8("最多串联 16 个效果器，请先移除不需要的效果器"));
        return;
    }
    if (graph == nullptr || instrumentNode == nullptr)
    {
        if (callback) callback(false, juce::String::fromUTF8("请先加载音源"));
        return;
    }
    const auto generation = loadGeneration;
    const auto effectGeneration = ++effectLoadGeneration;
    formatManager.createPluginInstanceAsync(description, sampleRate, bufferSize,
        [this, description, generation, effectGeneration, guard = lifetime, completion = std::move(callback)](std::unique_ptr<juce::AudioPluginInstance> instance,
                                                                                const juce::String& error) mutable
        {
            if (! guard->load(std::memory_order_acquire)) return;
            if (generation != loadGeneration || effectGeneration != effectLoadGeneration
                || graph == nullptr || instrumentNode == nullptr) return;
            if (instance == nullptr)
            {
                if (completion) completion(false, error.isNotEmpty() ? error : juce::String::fromUTF8("效果器加载失败"));
                return;
            }
            if (instance->getTotalNumInputChannels() < 1 || instance->getTotalNumOutputChannels() < 1)
            {
                if (completion) completion(false, juce::String::fromUTF8("此插件没有可用的音频输入或输出"));
                return;
            }
            const ScopedGraphPause pause(graph.get());
            auto effectNode = graph->addNode(std::move(instance));
            effectNodes.push_back({ effectNode, description });
            if (! rebuildConnections())
            {
                graph->removeNode(effectNode->nodeID);
                effectNodes.pop_back();
                rebuildConnections();
                // Resume without releasing the existing instrument.
                if (completion) completion(false, juce::String::fromUTF8("效果器音频通道不兼容，已恢复直接输出"));
                return;
            }
            // Resume without releasing the existing instrument.
            if (completion) completion(true, effectNode->getProcessor()->getName());
        });
}

void PluginHostEngine::unloadEffect()
{
    ++effectLoadGeneration;
    if (graph == nullptr || ! hasEffect()) return;
    const ScopedGraphPause pause(graph.get());
    effectEditorWindow.reset();
    for (const auto& effect : effectNodes) graph->removeNode(effect.node->nodeID);
    effectNodes.clear();
    rebuildConnections();
    // Resume without releasing the existing instrument.
}

EffectChainStates PluginHostEngine::captureEffectChain()
{
    EffectChainStates result;
    const ScopedGraphPause pause(graph.get());
    for (const auto& effect : effectNodes)
    {
        EffectChainState entry;
        entry.descriptionXml = effect.description.createXml()->toString();
        entry.bypassed = effect.node->isBypassed();
        effect.node->getProcessor()->getStateInformation(entry.state);
        result.push_back(std::move(entry));
    }
    // Resume without releasing the existing instrument.
    return result;
}

juce::MemoryBlock PluginHostEngine::captureInstrumentState()
{
    juce::MemoryBlock state;
    const ScopedGraphPause pause(graph.get());
    if (auto* plugin = getPlugin()) plugin->getStateInformation(state);
    // Resume without releasing the existing instrument.
    return state;
}

juce::String PluginHostEngine::getInstrumentDescriptionXml() const
{
    return hasPlugin() ? currentDescription.createXml()->toString() : juce::String();
}

juce::var PluginHostEngine::describeEffectChain() const
{
    juce::Array<juce::var> result;
    for (const auto& effect : effectNodes)
    {
        auto* row = new juce::DynamicObject();
        row->setProperty("name", effect.description.name);
        row->setProperty("version", effect.description.version);
        row->setProperty("bypassed", effect.node->isBypassed());
        row->setProperty("latencySamples", effect.node->getProcessor()->getLatencySamples());
        result.add(juce::var(row));
    }
    return result;
}

bool PluginHostEngine::removeEffectAt(int index)
{
    if (index < 0 || static_cast<size_t>(index) >= effectNodes.size() || ! graph) return false;
    ++effectLoadGeneration;
    const ScopedGraphPause pause(graph.get());
    effectEditorWindow.reset();
    graph->removeNode(effectNodes[static_cast<size_t>(index)].node->nodeID);
    effectNodes.erase(effectNodes.begin() + index);
    const auto ok = rebuildConnections();
    // Resume without releasing the existing instrument.
    return ok;
}

bool PluginHostEngine::moveEffect(int index, int destination)
{
    if (index < 0 || destination < 0 || static_cast<size_t>(index) >= effectNodes.size()
        || static_cast<size_t>(destination) >= effectNodes.size()) return false;
    ++effectLoadGeneration;
    const ScopedGraphPause pause(graph.get());
    auto slot = effectNodes[static_cast<size_t>(index)];
    effectNodes.erase(effectNodes.begin() + index);
    effectNodes.insert(effectNodes.begin() + destination, slot);
    const auto ok = rebuildConnections();
    // Resume without releasing the existing instrument.
    return ok;
}

bool PluginHostEngine::setEffectBypassedAt(int index, bool bypassed)
{
    if (index < 0 || static_cast<size_t>(index) >= effectNodes.size()) return false;
    const ScopedGraphPause pause(graph.get());
    effectNodes[static_cast<size_t>(index)].node->setBypassed(bypassed);
    const auto ok = rebuildConnections();
    // Resume without releasing the existing instrument.
    return ok;
}

bool PluginHostEngine::showEffectEditor(int index)
{
    if (index < 0 || static_cast<size_t>(index) >= effectNodes.size()) return false;
    effectEditorWindow.reset();
    auto* processor = effectNodes[static_cast<size_t>(index)].node->getProcessor();
    std::unique_ptr<juce::AudioProcessorEditor> editor(processor->createEditorAndMakeActive());
    if (! editor) editor = std::make_unique<juce::GenericAudioProcessorEditor>(*processor);
    effectEditorWindow = std::make_unique<PluginEditorWindow>(processor->getName(), std::move(editor));
    return true;
}

void PluginHostEngine::restoreEffectChain(const EffectChainStates& states, double sampleRate,
                                         int bufferSize, LoadCallback callback)
{
    if (! graph || ! instrumentNode || states.size() > 16)
    {
        if (callback) callback(false, juce::String::fromUTF8("效果链不可载入"));
        return;
    }
    struct Pending
    {
        EffectChainStates states;
        std::vector<juce::PluginDescription> descriptions;
        std::vector<std::unique_ptr<juce::AudioPluginInstance>> instances;
        LoadCallback completion;
    };
    auto pending = std::make_shared<Pending>();
    pending->states = states;
    pending->completion = std::move(callback);
    for (const auto& state : states)
    {
        juce::PluginDescription description;
        auto xml = juce::parseXML(state.descriptionXml);
        if (! xml || ! description.loadFromXml(*xml) || state.state.getSize() > 64u * 1024u * 1024u)
        {
            if (pending->completion) pending->completion(false, juce::String::fromUTF8("效果器状态损坏"));
            return;
        }
        pending->descriptions.push_back(description);
    }
    const auto generation = ++effectLoadGeneration;
    auto step = std::make_shared<std::function<void()>>();
    std::weak_ptr<std::function<void()>> weakStep = step;
    *step = [this, guard = lifetime, pending, weakStep, generation, sampleRate, bufferSize]()
    {
        if (! guard->load() || generation != effectLoadGeneration || ! graph) return;
        const auto index = pending->instances.size();
        if (index == pending->states.size())
        {
            const ScopedGraphPause pause(graph.get());
            effectEditorWindow.reset();
            auto previous = std::move(effectNodes);
            effectNodes.clear();
            for (size_t i = 0; i < index; ++i)
            {
                auto node = graph->addNode(std::move(pending->instances[i]));
                node->setBypassed(pending->states[i].bypassed);
                effectNodes.push_back({ node, pending->descriptions[i] });
            }
            const auto ok = rebuildConnections();
            if (ok)
                for (const auto& old : previous) graph->removeNode(old.node->nodeID);
            else
            {
                for (const auto& added : effectNodes) graph->removeNode(added.node->nodeID);
                effectNodes = std::move(previous);
                rebuildConnections();
            }
            // Resume without releasing the existing instrument.
            if (pending->completion) pending->completion(ok, ok ? juce::String::fromUTF8("效果链已恢复")
                : juce::String::fromUTF8("效果器通道不兼容，未替换原效果链"));
            return;
        }
        auto next = weakStep.lock();
        formatManager.createPluginInstanceAsync(pending->descriptions[index], sampleRate, bufferSize,
            [this, guard, pending, next, generation, index](std::unique_ptr<juce::AudioPluginInstance> instance,
                                                         const juce::String& error)
            {
                if (! guard->load() || generation != effectLoadGeneration) return;
                if (! instance || instance->getTotalNumInputChannels() < 1 || instance->getTotalNumOutputChannels() < 1)
                {
                    if (pending->completion) pending->completion(false, pending->descriptions[index].name + ": " + error);
                    return;
                }
                const auto& state = pending->states[index].state;
                if (state.getSize() > 0) instance->setStateInformation(state.getData(), static_cast<int>(state.getSize()));
                pending->instances.push_back(std::move(instance));
                if (next) (*next)();
            });
    };
    (*step)();
}

bool PluginHostEngine::setEffectBypassed(bool shouldBypass)
{
    return setEffectBypassedAt(0, shouldBypass);
}

bool PluginHostEngine::isEffectBypassed() const noexcept
{
    return hasEffect() && effectNodes.front().node->isBypassed();
}

bool PluginHostEngine::showPluginEditor(bool effect)
{
    if (effect) return showEffectEditor(0);
    auto node = instrumentNode;
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

void PluginHostEngine::closePluginEditor(bool effect)
{
    auto& window = effect ? effectEditorWindow : instrumentEditorWindow;
    // Destroying the editor gives container plug-ins a chance to commit the
    // selection made in their native UI before getStateInformation is called.
    window.reset();
}

bool PluginHostEngine::rebuildConnections()
{
    if (graph == nullptr || instrumentNode == nullptr || audioOutputNode == nullptr || midiInputNode == nullptr) return false;
    for (const auto& connection : graph->getConnections())
        graph->removeConnection(connection);
    const auto midiConnected = graph->addConnection(
        { { midiInputNode->nodeID, juce::AudioProcessorGraph::midiChannelIndex },
          { instrumentNode->nodeID, juce::AudioProcessorGraph::midiChannelIndex } });
    std::vector<juce::AudioProcessorGraph::Node::Ptr> chain;
    for (const auto& slot : effectNodes)
        chain.push_back(slot.node);
    return midiConnected && connectSerialEffects(*graph, instrumentNode, chain, audioOutputNode);
}

void PluginHostEngine::noteOn(int noteNumber, float velocity, double timestampSeconds) noexcept
{
    const auto note = juce::jlimit(0, 127, noteNumber);
    const auto kong = kongExpressionMode.load(std::memory_order_relaxed);
    velocity = performanceVelocity(kong, velocity);
    if (kong)
    {
        queue(kongBreathExpression(performanceChannel.load(std::memory_order_relaxed),
            lastBreathMidiValue.load(std::memory_order_relaxed)), timestampSeconds);
        bool playing = false;
        for (const auto& held : activeNotes) playing |= held.load(std::memory_order_relaxed);
        if (!playing)
            for (int channel = 1; channel <= 16; ++channel)
            {
                int selected = -1;
                for (const auto& route : kongTechniqueRoutes)
                    if (route.channel == channel && route.normalKey >= 0) { selected = route.normalKey; break; }
                for (size_t i = 0; i < kongTechniqueRoutes.size(); ++i)
                    if (kongTechniqueRoutes[i].channel == channel && kongTechniqueRoutes[i].keyswitch >= 0
                        && kongTechniqueInputs[i].load(std::memory_order_relaxed) > 0.5f)
                        selected = kongTechniqueRoutes[i].keyswitch;
                if (selected >= 0)
                {
                    queue(juce::MidiMessage::noteOn(channel, selected, static_cast<juce::uint8>(100)), timestampSeconds);
                    queue(juce::MidiMessage::noteOff(channel, selected), timestampSeconds);
                }
            }
    }
    activeNotes[static_cast<size_t>(note)].store(true, std::memory_order_relaxed);
    activeVelocities[static_cast<size_t>(note)].store(juce::jlimit(0.0f, 1.0f, velocity), std::memory_order_relaxed);
    queue(juce::MidiMessage::noteOn(performanceChannel.load(std::memory_order_relaxed), note,
                                    juce::jlimit(0.0f, 1.0f, velocity)), timestampSeconds);
}

void PluginHostEngine::noteOff(int noteNumber, double timestampSeconds) noexcept
{
    const auto note = juce::jlimit(0, 127, noteNumber);
    activeNotes[static_cast<size_t>(note)].store(false, std::memory_order_relaxed);
    queue(juce::MidiMessage::noteOff(performanceChannel.load(std::memory_order_relaxed), note), timestampSeconds);
    if (kongExpressionMode.load(std::memory_order_relaxed))
    {
        bool playing = false;
        for (const auto& held : activeNotes) playing |= held.load(std::memory_order_relaxed);
        // End the phrase, not each articulation change. Clear any sampler-held
        // notes in all configured slots without resetting pitch or effect tails.
        if (!playing)
            for (int channel = 1; channel <= 16; ++channel)
            {
                bool used = channel == performanceChannel.load(std::memory_order_relaxed);
                for (const auto& route : kongTechniqueRoutes) used |= route.channel == channel;
                if (used) queue(juce::MidiMessage::allNotesOff(channel), timestampSeconds);
            }
    }
}

void PluginHostEngine::breathChanged(float value, double timestampSeconds) noexcept
{
    const auto midiValue = juce::jlimit(0, 127, juce::roundToInt(performanceBreath(value) * 127.0f));
    if (lastBreathMidiValue.exchange(midiValue, std::memory_order_relaxed) == midiValue)
        return;
    // The controller's raw 0..127 value is routed to exactly one expression
    // destination. Sending CC2 and CC11 together can drive two mappings inside
    // SWAM and is the main cause of plateaus and conflicting expression curves.
    const auto channel = performanceChannel.load(std::memory_order_relaxed);
    if (kongExpressionMode.load(std::memory_order_relaxed))
        queue(kongBreathExpression(channel, midiValue), timestampSeconds);
    else
        queue(juce::MidiMessage::controllerEvent(channel,
            swamExpressionController.load(std::memory_order_relaxed), midiValue), timestampSeconds);
}

void PluginHostEngine::pitchBendChanged(float bipolarValue, double timestampSeconds) noexcept
{
    const auto pitch = juce::jlimit(0, 16383,
        juce::roundToInt(8192.0f + juce::jlimit(-1.0f, 1.0f, bipolarValue)
                         * (bipolarValue < 0.0f ? 8192.0f : 8191.0f)));
    lastPitchWheel.store(pitch, std::memory_order_relaxed);
    queue(juce::MidiMessage::pitchWheel(performanceChannel.load(std::memory_order_relaxed), pitch), timestampSeconds);
}

void PluginHostEngine::techniqueChanged(PerformanceTechnique technique, float value) noexcept
{
    const auto index = static_cast<size_t>(technique);
    if (index >= techniqueValues.size()) return;
    const auto safeValue = juce::jlimit(0.0f, 1.0f, value);
    if (kongExpressionMode.load(std::memory_order_relaxed))
    {
        const auto previous = kongTechniqueInputs[index].exchange(safeValue, std::memory_order_relaxed);
        const auto route = kongTechniqueRoutes[index];
        if (route.keyswitch >= 0 && (previous <= 0.5f) != (safeValue <= 0.5f))
        {
            int selected = route.normalKey;
            for (size_t i = 0; i < kongTechniqueRoutes.size(); ++i)
                if (kongTechniqueRoutes[i].channel == route.channel
                    && kongTechniqueRoutes[i].keyswitch >= 0
                    && kongTechniqueInputs[i].load(std::memory_order_relaxed) > 0.5f)
                    selected = kongTechniqueRoutes[i].keyswitch;
            if (selected >= 0)
            {
                // An inactive destination slot needs its articulation selected
                // before switchPerformanceChannel starts the held note there.
                if (performanceChannel.load(std::memory_order_relaxed) != route.channel)
                {
                    bool playing = false;
                    for (const auto& held : activeNotes) playing |= held.load(std::memory_order_relaxed);
                    if (playing)
                    {
                        queue(juce::MidiMessage::noteOn(route.channel, selected, static_cast<juce::uint8>(100)), 0.0);
                        queue(juce::MidiMessage::noteOff(route.channel, selected), 0.0);
                    }
                }
                changeKongArticulation(route.channel, selected,
                    [this, &route](int note) { return performanceChannel.load(std::memory_order_relaxed) == route.channel
                        && activeNotes[static_cast<size_t>(note)].load(std::memory_order_relaxed); },
                    [this](const auto& message) { queue(message, 0.0); });
            }
        }
        if (route.channel > 1)
        {
            int desired = 1;
            for (size_t routeIndex = 0; routeIndex < kongTechniqueRoutes.size(); ++routeIndex)
                if (kongTechniqueRoutes[routeIndex].channel > desired
                    && kongTechniqueInputs[routeIndex].load(std::memory_order_relaxed) > 0.5f)
                    desired = kongTechniqueRoutes[routeIndex].channel;
            switchPerformanceChannel(desired);
        }
        // Never fall through to guessed generic parameters for unsupported Qin techniques.
        return;
    }
    techniqueValues[index].store(safeValue, std::memory_order_relaxed);
    techniqueDirty[index].store(true, std::memory_order_release);
}

void PluginHostEngine::switchPerformanceChannel(int channel) noexcept
{
    channel = juce::jlimit(1, 16, channel);
    const auto previous = performanceChannel.exchange(channel, std::memory_order_relaxed);
    if (previous == channel) return;
    // Initialise the destination before retriggering a held note. Qin can latch
    // controller state at attack; sending expression after Note On is too late.
    const auto breathValue = juce::jmax(0, lastBreathMidiValue.load(std::memory_order_relaxed));
    queue(kongBreathExpression(channel, breathValue), 0.0);
    queue(juce::MidiMessage::pitchWheel(channel, lastPitchWheel.load(std::memory_order_relaxed)), 0.0);
    for (int note = 0; note < 128; ++note)
        if (activeNotes[static_cast<size_t>(note)].load(std::memory_order_relaxed))
        {
            queue(juce::MidiMessage::noteOff(previous, note), 0.0);
            queue(juce::MidiMessage::noteOn(channel, note,
                activeVelocities[static_cast<size_t>(note)].load(std::memory_order_relaxed)), 0.0);
        }
}

void PluginHostEngine::configureKongTechniqueProfile(const juce::String& key) noexcept
{
    for (auto& route : kongTechniqueRoutes) route = {};
    for (auto& value : kongTechniqueInputs) value.store(0.0f, std::memory_order_relaxed);
    switchPerformanceChannel(1);
    if (key.isEmpty()) return;
    player.beginOnsetTrace();
    juce::Logger::writeToLog("ONSET begin 30s bounded capture: " + key);
    const auto state = captureContainerState();
    if (state.getSize() < 9) return;
    const auto* bytes = static_cast<const char*>(state.getData());
    const auto length = juce::ByteOrder::littleEndianInt(bytes + 4);
    if (length > state.getSize() - 8) return;
    auto wrapper = juce::parseXML(juce::String::fromUTF8(bytes + 8, static_cast<int>(length)));
    if (!wrapper) return;
    auto* component = wrapper->getChildByName("IComponent");
    juce::MemoryBlock payload;
    if (!component || !payload.fromBase64Encoding(component->getAllSubText())) return;
    auto tree = juce::ValueTree::readFromData(payload.getData(), payload.getSize());
    const auto bind = [this, &tree](PerformanceTechnique technique, const juce::StringArray& names)
    {
        const auto binding = findKongTechnique(tree, names);
        kongTechniqueRoutes[static_cast<size_t>(technique)] = {binding.channel, binding.keyswitch, binding.normalKey};
    };
    bind(PerformanceTechnique::vibrato, {"Vib_Main", "Vib_Layers", "Vib_Mellow", "Vib_Bright"});
    bind(PerformanceTechnique::flutter, {"Flutter", "Flutter2"});
    bind(PerformanceTechnique::tremolo, {"Main_Shake_Mellow", "Roll", "Sus_Main_Roll", "Tremolo"});
    bind(PerformanceTechnique::pizzicato, {"Pizz"});
}

void PluginHostEngine::resetPerformance() noexcept
{
    lastBreathMidiValue.store(-1, std::memory_order_relaxed);
    player.requestPerformanceReset();
    performanceChannel.store(1, std::memory_order_relaxed);
    lastPitchWheel.store(8192, std::memory_order_relaxed);
    for (auto& note : activeNotes) note.store(false, std::memory_order_relaxed);
    for (size_t index = 0; index < techniqueValues.size(); ++index)
    {
        kongTechniqueInputs[index].store(0.0f, std::memory_order_relaxed);
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

juce::MemoryBlock PluginHostEngine::captureKongState()
{
    if (! hasPlugin() || fengyin::SupportedInstrumentClassifier::classify(currentDescription.name,
        currentDescription.manufacturerName, currentDescription.fileOrIdentifier)
            != fengyin::SupportedInstrumentClassifier::Brand::kong) return {};
    return captureContainerState();
}

juce::MemoryBlock PluginHostEngine::captureContainerState()
{
    juce::MemoryBlock state;
    if (! hasPlugin()) return state;
    // Do not detach the audible instance merely to save it. QinEngine can lose
    // its live rack/output routing during that detach/attach cycle. JUCE's
    // suspended flag prevents processBlock from racing this message-thread save.
    auto* processor = getPlugin();
    const juce::ScopedLock lock(processor->getCallbackLock());
    processor->suspendProcessing(true);
    processor->getStateInformation(state);
    processor->suspendProcessing(false);
    return state;
}

bool PluginHostEngine::restoreKongProject(const juce::MemoryBlock& project)
{
    if (! hasPlugin()
        || fengyin::SupportedInstrumentClassifier::classify(currentDescription.name,
            currentDescription.manufacturerName, currentDescription.fileOrIdentifier)
            != fengyin::SupportedInstrumentClassifier::Brand::kong)
        return false;
    const auto state = KongProjectFile::toPluginState(project);
    return state.getSize() > 0 && restoreContainerState(state);
}

bool PluginHostEngine::restoreContainerState(const juce::MemoryBlock& state)
{
    if (! hasPlugin() || state.getSize() == 0 || state.getSize() > 16 * 1024 * 1024) return false;
    auto* processor = getPlugin();
    const juce::ScopedLock lock(processor->getCallbackLock());
    processor->suspendProcessing(true);
    processor->setStateInformation(state.getData(), static_cast<int>(state.getSize()));
    processor->suspendProcessing(false);
    resetPerformance();
    return true; // setStateInformation has no result; soundbank availability is verified by the caller.
}

bool PluginHostEngine::setBendRange(int semitones)
{
    auto* plugin = getPlugin();
    if (plugin == nullptr || semitones < 1 || semitones > 4) return false;
    struct Change { juce::AudioProcessorParameter* parameter; float value; float old; };
    std::vector<Change> changes;
    bool foundUp = false, foundDown = false;
    for (auto* parameter : plugin->getParameters())
    {
        if (parameter == nullptr) continue;
        const auto role = BendRangeParameters::role(parameter->getName(160));
        if (role == BendRangeParameters::none) continue;
        const auto value = BendRangeParameters::valueFor(*parameter, semitones);
        if (! value) return false;
        changes.push_back({ parameter, *value, parameter->getValue() });
        foundUp |= role == BendRangeParameters::up || role == BendRangeParameters::both;
        foundDown |= role == BendRangeParameters::down || role == BendRangeParameters::both;
    }
    if (! foundUp || ! foundDown) return false;
    const juce::ScopedLock lock(plugin->getCallbackLock());
    pitchBendChanged(0.0f);
    for (const auto& change : changes)
    {
        change.parameter->beginChangeGesture();
        change.parameter->setValueNotifyingHost(change.value);
        change.parameter->endChangeGesture();
    }
    for (const auto& change : changes)
        if (! BendRangeParameters::displays(change.parameter->getText(change.parameter->getValue(), 128), semitones))
        {
            for (const auto& rollback : changes) rollback.parameter->setValueNotifyingHost(rollback.old);
            return false;
        }
    return true;
}

bool PluginHostEngine::selectProgramByAliases(const juce::StringArray& aliases)
{
    auto* processor = getPlugin();
    if (processor == nullptr) return false;
    for (int index = 0; index < processor->getNumPrograms(); ++index)
    {
        const auto candidate = processor->getProgramName(index).toLowerCase().removeCharacters(" ._-");
        for (const auto& alias : aliases)
            if (candidate == alias.toLowerCase().removeCharacters(" ._-"))
            {
                processor->setCurrentProgram(index);
                return processor->getCurrentProgram() == index;
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
    if (instrumentModelParameter == nullptr || instrumentModelNames.size() < 2
        || instrumentModelValues.size() != static_cast<size_t>(instrumentModelNames.size())) return -1;
    const auto value = instrumentModelParameter->getValue();
    int nearest = 0;
    auto distance = std::abs(value - instrumentModelValues.front());
    for (int index = 1; index < instrumentModelNames.size(); ++index)
        if (const auto candidate = std::abs(value - instrumentModelValues[static_cast<size_t>(index)]);
            candidate < distance)
        {
            nearest = index;
            distance = candidate;
        }
    return nearest;
}

juce::String PluginHostEngine::getCurrentInstrumentModelName() const
{
    const auto index = getCurrentInstrumentModelIndex();
    return juce::isPositiveAndBelow(index, instrumentModelNames.size()) ? instrumentModelNames[index] : juce::String();
}

bool PluginHostEngine::selectInstrumentModel(int index)
{
    const ScopedGraphPause pause(graph.get());
    juce::Logger::writeToLog("Instrument model selection begin; index=" + juce::String(index));
    if (instrumentModelParameter == nullptr || ! juce::isPositiveAndBelow(index, instrumentModelNames.size())
        || instrumentModelValues.size() != static_cast<size_t>(instrumentModelNames.size()))
        return false;
    const auto value = instrumentModelValues[static_cast<size_t>(index)];
    if (getCurrentInstrumentModelIndex() == index)
    {
        juce::Logger::writeToLog("Instrument model unchanged; no write");
        return true;
    }
    instrumentModelParameter->beginChangeGesture();
    instrumentModelParameter->setValueNotifyingHost(value);
    instrumentModelParameter->endChangeGesture();
    juce::Logger::writeToLog("Instrument model selection complete");
    return true;
}

int PluginHostEngine::applySwamToneProfile(const SwamToneProfile& profile, bool restoreModel)
{
    const ScopedGraphPause pause(graph.get());
    swamToneAudit = juce::var();
    auto* plugin = getPlugin();
    if (plugin == nullptr || ! profile.enabled) return 0;
    if (! profile.releaseParameters.isEmpty())
    {
        auto* audit = new juce::DynamicObject();
        swamToneAudit = juce::var(audit);
        audit->setProperty("expectedAtApply", profile.releaseParameters.size());
        audit->setProperty("verifiedAtApply", 0);
        audit->setProperty("modelApplied", false);
        // Some SWAM VST3 builds append a build/revision suffix to the semantic
        // version (for example 3.9.4.x).  Treat those as the same compatible
        // release instead of incorrectly warning a 3.9.4 user.
        if (! currentDescription.version.trim().startsWith("3.9.4")) return 0;
        const auto retired = retiredToneParameters(previousReleaseParameters,
            profile.releaseParameters, releaseBaseline);
        const auto model = instrumentModelNames.indexOf(profile.releaseModel);
        const auto modelApplied = !restoreModel || (model >= 0 && selectInstrumentModel(model));
        if (!retired.isEmpty()) restoreToneParameters(retired);
        const auto restored = restoreToneParameters(profile.releaseParameters);
        previousReleaseParameters = profile.releaseParameters;
        int verified = 0;
        const auto actual = captureToneParameters();
        for (const auto& expected : profile.releaseParameters)
            for (const auto& value : actual)
                if (expected.identifier == value.identifier && std::abs(expected.value - value.value) < 0.0001f)
                { ++verified; break; }
        audit->setProperty("verifiedAtApply", verified);
        audit->setProperty("modelApplied", modelApplied);
        return restored;
    }
    if (! profile.displayTargets.empty())
    {
        SwamToneApplication application;
        const auto verified = application.apply(plugin->getParameters(), profile);
        swamToneAudit = application.diagnostic();
        return verified;
    }
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
    for (const auto& effect : effectNodes)
        if (! effect.node->isBypassed())
            samples += effect.node->getProcessor()->getLatencySamples();
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
    instrumentModelValues.clear();
    if (instrumentNode == nullptr) return;
    int bestScore = 0;
    for (auto* parameter : instrumentNode->getProcessor()->getParameters())
    {
        if (parameter == nullptr) continue;
        const auto name = normalisedParameterName(*parameter);
        if (isProtectedPerformanceParameter(name)) continue;
        int score = 0;
        if (name == "instrumentmodel" || name == "saxmodel" || name == "instrumentbody") score = 120;
        else if (name == "instrument" || name == "model" || name == "body") score = 100;
        else if (name.contains("instrumentmodel") || name.contains("saxmodel")) score = 90;
        else if (name == "bodymodel" || name == "bodytype" || name == "instrumenttype") score = 80;
        if (score <= bestScore) continue;
        const auto steps = parameter->getNumSteps();
        juce::StringArray names;
        std::vector<float> values;
        const auto collectAt = [&] (float value)
        {
            auto label = parameter->getText(value, 96).trim();
            if (label.isEmpty() || (! names.isEmpty() && names[names.size() - 1] == label)) return;
            names.add(label);
            values.push_back(value);
        };
        if (steps >= 2 && steps <= 128)
            for (int index = 0; index < steps; ++index)
                collectAt(static_cast<float>(index) / static_cast<float>(steps - 1));
        else
        {
            // Some SWAM VST3 builds expose the instrument/body selector as a
            // continuous parameter even though its text is a discrete list.
            // Sample the host-visible text so the same choices shown by the
            // factory UI remain available in FengYin.
            for (int index = 0; index <= 512; ++index)
                collectAt(static_cast<float>(index) / 512.0f);
        }
        auto distinctNames = names;
        distinctNames.removeDuplicates(false);
        if (distinctNames.size() < 2 || distinctNames.size() > 64) continue;
        bestScore = score;
        instrumentModelParameter = parameter;
        instrumentModelNames = names;
        instrumentModelValues = std::move(values);
    }
}

void PluginHostEngine::queue(juce::MidiMessage message, double timestampSeconds) noexcept
{
    (void) player.enqueueMidi(message, timestampSeconds);
}
}
