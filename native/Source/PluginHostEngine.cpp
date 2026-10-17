#include "PluginHostEngine.h"
#include "KensaVibratoRange.h"
#include "KensaBendRange.h"
#include "TechniqueMidiDeduplication.h"
#include "SwamTechniqueRouting.h"
#include "SwamStateCompatibility.h"
#include "SimpleTechniquePolicy.h"
#include "SwamPluginClassifier.h"
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
    TechniqueProcessingGraph() { filteredMidi.ensureSize(256 * 1024); }
    std::function<void()> applyTechniqueValues;
    std::function<bool(const juce::MidiMessage&)> suppressTechniqueMidi;
    std::function<void(juce::MidiBuffer&,int,double)> transformPerformanceMidi;
    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& messages) override
    {
        if (applyTechniqueValues) applyTechniqueValues();
        if (suppressTechniqueMidi)
        {
            filteredMidi.clear();
            for (const auto metadata : messages)
                if (!suppressTechniqueMidi(metadata.getMessage()))
                    filteredMidi.addEvent(metadata.data,metadata.numBytes,metadata.samplePosition);
            messages.swapWith(filteredMidi);
        }
        if (transformPerformanceMidi) transformPerformanceMidi(messages,buffer.getNumSamples(),getSampleRate());
        juce::AudioProcessorGraph::processBlock(buffer, messages);
    }
private:
    juce::MidiBuffer filteredMidi;
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
        // Editors must not cover the host's save/confirmation dialogs.
        setAlwaysOnTop(false);
        keepTitleBarOnScreen();
        setVisible(true);
        keepTitleBarOnScreen();
        toFront(true);
        grabKeyboardFocus();
    }
    void keepTitleBarOnScreen()
    {
        const auto* display = juce::Desktop::getInstance().getDisplays().getDisplayForRect(getBounds());
        if (display == nullptr) return;
        const auto area = display->userArea;
        // Do not resize fixed-size third-party content. Even an oversized editor
        // must leave its native title bar reachable in the selected work area.
        juce::BorderSize<int> frame;
        if (const auto* peer = getPeer())
            if (const auto nativeFrame = peer->getFrameSizeIfPresent()) frame = *nativeFrame;
        const int left = area.getX() + frame.getLeft(), top = area.getY() + frame.getTop();
        setTopLeftPosition(juce::jlimit(left, juce::jmax(left, area.getRight() - frame.getRight() - getWidth()), getX()),
                           juce::jlimit(top, juce::jmax(top, area.getBottom() - frame.getBottom() - getHeight()), getY()));
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
                                 LoadCallback callback, bool verifyStateRoundtrip)
{
    parkCurrentContainer();
    unload();
    const auto generation = loadGeneration;
    std::shared_ptr<CachedContainer> cached;
    const auto cacheKey = description.createIdentifierString();
    if (const auto found = containerCache.find(cacheKey); found != containerCache.end()
        && (initialState.getSize() > 0 || found->second->instance != nullptr))
    {
        cached = found->second;
        containerCache.erase(found);
    }
    juce::Logger::writeToLog("Plugin load request " + juce::String(generation) + ": " + description.name);
    auto finish = [this, description, sampleRate, bufferSize, initialState, generation, verifyStateRoundtrip, cached,
         guard = lifetime, completion = std::move(callback)]
        (std::unique_ptr<juce::AudioPluginInstance> instance, const juce::String& error) mutable
        {
            if (! guard->load(std::memory_order_acquire))
                return;
            if (generation != loadGeneration) return;
            juce::Logger::writeToLog("Plugin instance ready " + juce::String(generation));
            auto* restoredProcessor = cached && cached->graph
                ? dynamic_cast<juce::AudioPluginInstance*>(cached->instrument->getProcessor()) : instance.get();
            if (restoredProcessor == nullptr)
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
                restoredProcessor->setStateInformation(initialState.getData(),
                                              static_cast<int>(initialState.getSize()));
                if(verifyStateRoundtrip) {
                    juce::MemoryBlock returned;restoredProcessor->getStateInformation(returned);
                    if(!SwamStateCompatibility::preserved(initialState,returned)) {
                        if(completion)completion(false,juce::String::fromUTF8("SWAM跨版本恢复未通过完整参数与曲线回读，未启用该方案；请用本机版本重新导出。"));
                        return;
                    }
                    juce::Logger::writeToLog("SWAM 3.8.2 -> 3.9.4 complete parameter and MIDI curve roundtrip verified");
                }
                juce::Logger::writeToLog("Plugin state restore returned");
            }
            restoredProcessor->suspendProcessing(false);
            if (cached && cached->graph)
            {
                graph = std::move(cached->graph);
                instrumentNode = cached->instrument;
                audioOutputNode = cached->output;
                midiInputNode = cached->midi;
                graph->setPlayConfigDetails(0, 2, sampleRate, bufferSize);
                juce::Logger::writeToLog("Reusing cached container graph: " + description.name);
            }
            else
            {
            auto processingGraph = std::make_unique<TechniqueProcessingGraph>();
            processingGraph->applyTechniqueValues = [this] { flushTechniqueValues(); };
            processingGraph->transformPerformanceMidi = [this](juce::MidiBuffer& messages,int count,double rate) {
                suonaDualVoice.process(messages,count,rate);
                const double now = juce::Time::getMillisecondCounterHiRes() * .001;
                for (const auto event : messages)
                    destinationTrace.record(event.getMessage(), now + event.samplePosition / juce::jmax(1.0,rate));
            };
            processingGraph->suppressTechniqueMidi = [this](const juce::MidiMessage& message)
            {
                if (kongExpressionMode.load() || kontaktStandardControls.load() || !message.isController()) return false;
                const int route=(message.getChannel()<<8)|(message.getControllerNumber()+1);
                if (route==((swamExpressionChannel.load()<<8)|(swamExpressionController.load()+1))) return false;
                for (const auto technique : {PerformanceTechnique::vibrato,PerformanceTechnique::growl,PerformanceTechnique::flutter})
                {
                    const auto index=static_cast<size_t>(technique);
                    if (nativeTechniqueRoutes[index].load()==route && techniqueValues[index].load()==0.0f) return true;
                }
                return false;
            };
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
            }
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
        };
    if (cached)
        finish(std::move(cached->instance), {});
    else if (containersWarming.contains(cacheKey))
        containerWarmWaiters[cacheKey] = std::move(finish);
    else
        formatManager.createPluginInstanceAsync(description, sampleRate, bufferSize, std::move(finish));
}

void PluginHostEngine::parkCurrentContainer()
{
    if (!graph || !instrumentNode) return;
    const auto brand = SupportedInstrumentClassifier::classify(currentDescription.name,
        currentDescription.manufacturerName, currentDescription.fileOrIdentifier);
    if (brand != SupportedInstrumentClassifier::Brand::kong
        && brand != SupportedInstrumentClassifier::Brand::kontakt) return;
    player.setProcessor(nullptr); // Synchronise with the device before changing ownership.
    instrumentEditorWindow.reset();
    effectEditorWindow.reset();
    graph->suspendProcessing(true);
    for (const auto& effect : effectNodes) graph->removeNode(effect.node->nodeID);
    effectNodes.clear();
    auto entry = std::make_shared<CachedContainer>();
    entry->graph = std::move(graph);
    entry->instrument = instrumentNode;
    entry->output = audioOutputNode;
    entry->midi = midiInputNode;
    const auto key = currentDescription.createIdentifierString();
    if (!containerCache.contains(key) && containerCache.size() >= 2)
        containerCache.erase(containerCache.begin());
    containerCache[key] = std::move(entry);
}

void PluginHostEngine::prewarmContainers(const juce::Array<juce::PluginDescription>& descriptions,
                                        double sampleRate, int bufferSize)
{
    for (const auto& description : descriptions)
    {
        const auto brand = SupportedInstrumentClassifier::classify(description.name,
            description.manufacturerName, description.fileOrIdentifier);
        if (brand != SupportedInstrumentClassifier::Brand::kong
            && brand != SupportedInstrumentClassifier::Brand::kontakt) continue;
        const auto key = description.createIdentifierString();
        if (key == getPluginIdentifier() || containerCache.contains(key) || containersWarming.contains(key)) continue;
        if (containerCache.size() + containersWarming.size() >= 2) break;
        containersWarming.insert(key);
        formatManager.createPluginInstanceAsync(description, sampleRate, bufferSize,
            [this, key, sampleRate, bufferSize, guard = lifetime](std::unique_ptr<juce::AudioPluginInstance> instance,
                                                               const juce::String& error)
            {
                if (!guard->load(std::memory_order_acquire)) return;
                containersWarming.erase(key);
                if (const auto waiter = containerWarmWaiters.find(key); waiter != containerWarmWaiters.end())
                {
                    auto finish = std::move(waiter->second);
                    containerWarmWaiters.erase(waiter);
                    if (instance)
                    {
                        instance->setPlayConfigDetails(0, 2, sampleRate, bufferSize);
                        instance->prepareToPlay(sampleRate, bufferSize);
                        instance->suspendProcessing(true);
                    }
                    finish(std::move(instance), error);
                    return;
                }
                if (!instance || key == getPluginIdentifier() || containerCache.contains(key)
                    || containerCache.size() >= 2)
                {
                    if (!instance) juce::Logger::writeToLog("Container prewarm unavailable: " + error);
                    return;
                }
                instance->setPlayConfigDetails(0, 2, sampleRate, bufferSize);
                instance->prepareToPlay(sampleRate, bufferSize);
                instance->suspendProcessing(true);
                auto entry = std::make_shared<CachedContainer>();
                entry->instance = std::move(instance);
                containerCache[key] = std::move(entry);
                juce::Logger::writeToLog("Container prewarm ready: " + key);
            });
    }
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
    suonaDualVoice.configure(false);
    kontaktStandardControls.store(false);
    stringTremolo.clear();
    for (auto& parameter : techniqueParameters) parameter.store(nullptr);
    for (auto& dirty : techniqueDirty) dirty.store(false, std::memory_order_relaxed);
    instrumentModelParameter = nullptr;
    instrumentModelNames.clear();
    instrumentModelValues.clear();
    swamExpressionController.store(11, std::memory_order_relaxed);
    swamExpressionChannel.store(1, std::memory_order_relaxed);
    for (auto& route : nativeTechniqueRoutes) route.store(0, std::memory_order_relaxed);
    vibratoRateParameter.store(nullptr);automaticVibratoActive.store(false);
    juce::Logger::writeToLog("Plugin unload complete");
    for (auto& value : lastNativeTechniqueMessages) value.store(0, std::memory_order_relaxed);
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
        const bool tempoOverride=parameter==vibratoRateParameter.load()&&automaticVibratoActive.load();
        const auto expected=tempoVibratoRates[MusicTempoPolicy::valid(musicTempoMode.load())];
        const auto value=tempoOverride&&std::abs(parameter->getValue()-expected)<.0001f?originalVibratoRate:parameter->getValue();
        result.add({ stableParameterIdentifier(*parameter, index), value });
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
    for (auto& value : lastNativeTechniqueMessages) value.store(0, std::memory_order_relaxed);
    swamExpressionController.store(11, std::memory_order_relaxed);
    swamExpressionChannel.store(1, std::memory_order_relaxed);
    for (auto& route : nativeTechniqueRoutes) route.store(0, std::memory_order_relaxed);
    auto* plugin = getPlugin();
    if (plugin == nullptr) return false;

    juce::MemoryBlock state;
    plugin->getStateInformation(state);
    const auto original=state;
    const std::string_view key=SwamPluginClassifier::instrumentKey(plugin->getName().toStdString());
    const bool wantFlutter=simpleOther(key)==PerformanceTechnique::flutter;
    const bool ownTremolo=key=="violin" || key=="cello";
    const auto configured=SwamTechniqueRouting::prepare(state,simpleVibrato(key),simpleGrowl(key),wantFlutter,true,ownTremolo);
    if(configured.supported && configured.changed)
    {
        // Once per activation, while the graph is paused, never on each CC or mode switch.
        plugin->setStateInformation(state.getData(),static_cast<int>(state.getSize()));
        juce::MemoryBlock verified;plugin->getStateInformation(verified);
        auto check=verified;
        const auto routing=SwamTechniqueRouting::prepare(check,simpleVibrato(key),simpleGrowl(key),wantFlutter,true,ownTremolo);
        if(!routing.supported || routing.changed || routing.vibrato!=configured.vibrato || routing.growl!=configured.growl || routing.flutter!=configured.flutter || routing.volume!=configured.volume)
        {
            plugin->setStateInformation(original.getData(),static_cast<int>(original.getSize()));
            state=original;
            juce::Logger::writeToLog("SWAM automatic technique routing failed verification; original state restored");
        }
        else {state=verified;juce::Logger::writeToLog("SWAM automatic technique routes verified; curves preserved");}
    }
    const auto result = SwamExpressionCurve::applyToState(state);
    for (const auto& route : result.routes)
    {
        const auto parameter = route.parameter.toLowerCase().removeCharacters(" ._-");
        if (parameter == "expression") swamExpressionChannel.store(route.channel);
        const std::pair<const char*, PerformanceTechnique> names[] = {
            {"vibratodepth", PerformanceTechnique::vibrato}, {"growl", PerformanceTechnique::growl},
            {"flutter", PerformanceTechnique::flutter}, {"portamento", PerformanceTechnique::portamento},
            {"tremolo", PerformanceTechnique::tremolo}, {"mute", PerformanceTechnique::mute},
            {"halfvalve", PerformanceTechnique::halfValve}, {"pizzicato", PerformanceTechnique::pizzicato},
            {"breathnoise", PerformanceTechnique::breathNoise}, {"bowpressure", PerformanceTechnique::bowPressure},
            {"overblow", PerformanceTechnique::overblow}, {"fall", PerformanceTechnique::fall},
            {"alternatefingering", PerformanceTechnique::alternateFingering}, {"legato", PerformanceTechnique::legato}
        };
        for (const auto& name : names)
            if (parameter == name.first)
                nativeTechniqueRoutes[static_cast<size_t>(name.second)].store((route.channel << 8) | (route.controller + 1));
    }
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
    lastBreathMidiValue.store(-1, std::memory_order_relaxed);
    if(key=="violin" || key=="cello")stringTremolo.prepare(*plugin);
    else stringTremolo.clear();
    prepareTempoVibrato(*plugin);
    for (const auto technique : {PerformanceTechnique::vibrato, PerformanceTechnique::growl, PerformanceTechnique::flutter})
        techniqueChanged(technique, 0.0f);
    // Activation only discovers routing. Saved/native curves remain authoritative;
    // never write a synthetic curve back into the live instrument here.
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
    if (auto* plugin = getPlugin()) {
        auto* rate=vibratoRateParameter.load();const float current=rate?rate->getValue():0;
        const bool overridden=rate&&automaticVibratoActive.load()&&std::abs(current-tempoVibratoRates[MusicTempoPolicy::valid(musicTempoMode.load())])<.0001f;
        if(overridden)rate->setValueNotifyingHost(originalVibratoRate);
        plugin->getStateInformation(state);
        if(overridden)rate->setValueNotifyingHost(current);
    }
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
        window->keepTitleBarOnScreen();
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
    const juce::SpinLock::ScopedLockType eventLock(performanceEventLock);
    const auto note = juce::jlimit(0, 127, noteNumber);
    const auto kong = kongExpressionMode.load(std::memory_order_relaxed);
    velocity = performanceVelocity(kong, velocity);
    if (kong)
    {
        queue(kongBreathExpression(performanceChannel.load(std::memory_order_relaxed),
            lastBreathMidiValue.load(std::memory_order_relaxed)), timestampSeconds);
        if(kongVibratoCC.load()==1)
            queue(juce::MidiMessage::controllerEvent(performanceChannel.load(),1,
                juce::roundToInt(kongTechniqueInputs[static_cast<size_t>(PerformanceTechnique::vibrato)].load()*127.f)),timestampSeconds);
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
    const juce::SpinLock::ScopedLockType eventLock(performanceEventLock);
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
    const juce::SpinLock::ScopedLockType eventLock(performanceEventLock);
    const auto midiValue = juce::jlimit(0, 127, juce::roundToInt(value * 127.0f));
    if (lastBreathMidiValue.exchange(midiValue, std::memory_order_relaxed) == midiValue)
        return;
    // The controller's raw 0..127 value is routed to exactly one expression
    // destination. Sending CC2 and CC11 together can drive two mappings inside
    // SWAM and is the main cause of plateaus and conflicting expression curves.
    const auto channel = performanceChannel.load(std::memory_order_relaxed);
    if (kongExpressionMode.load(std::memory_order_relaxed))
        queue(kongBreathExpression(channel, midiValue), timestampSeconds);
    else
        queue(juce::MidiMessage::controllerEvent(swamExpressionChannel.load(std::memory_order_relaxed),
            swamExpressionController.load(std::memory_order_relaxed), midiValue), timestampSeconds);
}

void PluginHostEngine::pitchBendChanged(float bipolarValue, double timestampSeconds) noexcept
{
    const juce::SpinLock::ScopedLockType eventLock(performanceEventLock);
    const auto pitch = juce::jlimit(0, 16383,
        juce::roundToInt(8192.0f + juce::jlimit(-1.0f, 1.0f, bipolarValue)
                         * (bipolarValue < 0.0f ? 8192.0f : 8191.0f)));
    lastPitchWheel.store(pitch, std::memory_order_relaxed);
    queue(juce::MidiMessage::pitchWheel(performanceChannel.load(std::memory_order_relaxed), pitch), timestampSeconds);
}

void PluginHostEngine::prepareTempoVibrato(juce::AudioPluginInstance& plugin)
{
    vibratoRateParameter.store(nullptr);automaticVibratoActive.store(false);
    const std::string_view key=SwamPluginClassifier::instrumentKey(plugin.getName().toStdString());
    for(auto* p:plugin.getParameters())if(p->getName(100)=="Vibrato Rate") {
        originalVibratoRate=p->getValue();
        for(int mode=0;mode<3;++mode){
            const auto policy=MusicTempoPolicy::forMode(mode);const float wanted=(key=="violin"||key=="cello")?policy.stringsRate:policy.saxRate;
            float best=-1;double delta=1e9;float lo=0,hi=1;
            const auto lowValue=p->getText(0,100).getDoubleValue(),highValue=p->getText(1,100).getDoubleValue();
            if(wanted>=juce::jmin(lowValue,highValue)&&wanted<=juce::jmax(lowValue,highValue))
              for(int i=0;i<28;++i){float n=(lo+hi)*.5f;const auto actual=p->getText(n,100).getDoubleValue();double diff=std::abs(actual-wanted);if(diff<delta){delta=diff;best=n;}if(diff<.001)break;if((actual<wanted)==(lowValue<highValue))lo=n;else hi=n;}
            tempoVibratoRates[mode]=delta<.05?best:-1;
        }
        vibratoRateParameter.store(p);tempoRateDirty.store(true);break;
    }
}

void PluginHostEngine::techniqueChanged(PerformanceTechnique technique,float value) noexcept
{
    if(technique==PerformanceTechnique::vibrato){if(automaticVibratoActive.exchange(false))tempoRateDirty.store(true);}
    routeTechnique(technique,value);
}

void PluginHostEngine::automaticVibratoChanged(float input,float fade) noexcept
{
    if(!hasIndependentTechniqueRoute(PerformanceTechnique::vibrato)){routeTechnique(PerformanceTechnique::vibrato,0);return;}
    if(!automaticVibratoActive.exchange(true))tempoRateDirty.store(true);
    input=std::isfinite(input)?juce::jlimit(0.f,1.f,input):0;fade=std::isfinite(fade)?juce::jlimit(0.f,1.f,fade):0;
    // Use the established CC input fade. Tempo changes timing/rate only;
    // SWAM remains responsible for the preset's technique response curve.
    routeTechnique(PerformanceTechnique::vibrato,input*fade);
}

void PluginHostEngine::routeTechnique(PerformanceTechnique technique, float value) noexcept
{
    const juce::SpinLock::ScopedLockType eventLock(performanceEventLock);
    const auto index = static_cast<size_t>(technique);
    if (index >= techniqueValues.size()) return;
    const auto safeValue = juce::jlimit(0.0f, 1.0f, value);
    if(technique==PerformanceTechnique::tremolo && !kongExpressionMode.load()) {
        // Never fall back to a similarly named "Tremolo Mode" parameter if
        // this plug-in/version failed the exact native articulation checks.
        if(stringTremolo.available())stringTremolo.request(safeValue>.5f);
        return;
    }
    if (kontaktStandardControls.load()) {
        if(technique==PerformanceTechnique::vibrato) {
            // Kenny G PRO: <=30 is the vibrato dead zone; >96 also adds
            // a pitch offset. Preserve true OFF and use only the verified
            // periodic-vibrato interval, capped at the user's verified 70.
            // This profile is Kensa-only.
            const int midiValue=kensaVibratoController(safeValue);
            sendTechniqueController(lastNativeTechniqueMessages[index],(1<<8)|2,midiValue,true,[&] {
                return queue(juce::MidiMessage::controllerEvent(1,1,midiValue),0.0);
            });
        }
        return;
    }
    if (kongExpressionMode.load(std::memory_order_relaxed))
    {
        const auto previous = kongTechniqueInputs[index].exchange(safeValue, std::memory_order_relaxed);
        if (suonaDualVoice.active() && technique==PerformanceTechnique::flutter) {
            suonaDualVoice.setFlutter(safeValue>.5f);
            return;
        }
        if(kongProfile.enabled && technique==PerformanceTechnique::vibrato) {
            if(kongVibratoCC.load()==1)
                queue(juce::MidiMessage::controllerEvent(performanceChannel.load(),1,juce::roundToInt(safeValue*127.f)),0.0);
            return; // Continuous modulation, never a vibrato keyswitch/restart.
        }
        const auto route = kongTechniqueRoutes[index];
        // A secondary slot already contains its chosen articulation. On
        // release, switch back without restarting the outgoing technique slot.
        if (route.keyswitch >= 0 && (route.channel <= 1 || safeValue > 0.5f)
            && (previous <= 0.5f) != (safeValue <= 0.5f))
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
                    [this](const auto& message) { queue(message, 0.0); },
                    [this](int note) { return activeVelocities[static_cast<size_t>(note)].load(std::memory_order_relaxed); });
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
    // OFF/wait is an explicit gate, not CC0 through a potentially non-zero
    // minimum curve. Active values still travel through the original curve.
    if (safeValue == 0.0f && (technique == PerformanceTechnique::vibrato
        || technique == PerformanceTechnique::growl || technique == PerformanceTechnique::flutter))
    {
        lastNativeTechniqueMessages[index].store(0, std::memory_order_relaxed);
        techniqueDirty[index].store(true, std::memory_order_release);
        return;
    }
    // A direct depth parameter write bypasses SWAM's mapping curve. Never
    // advertise the simplified SWAM modes without an independent MIDI route.
    if ((technique == PerformanceTechnique::vibrato || technique == PerformanceTechnique::growl)
        && !hasIndependentTechniqueRoute(technique)) return;
    if (const int route = nativeTechniqueRoutes[index].load(std::memory_order_relaxed))
    {
        bool unique = route != ((swamExpressionChannel.load() << 8) | (swamExpressionController.load() + 1));
        for (size_t other = 0; other < nativeTechniqueRoutes.size(); ++other)
            if (other != index && nativeTechniqueRoutes[other].load() == route) unique = false;
        const auto value = juce::roundToInt(safeValue * 127.0f);
        sendTechniqueController(lastNativeTechniqueMessages[index], route, value, unique, [&] {
            return queue(juce::MidiMessage::controllerEvent(route >> 8, (route & 255) - 1, value), 0.0);
        });
        return;
    }
    techniqueDirty[index].store(true, std::memory_order_release);
}

void PluginHostEngine::switchPerformanceChannel(int channel) noexcept
{
    channel = juce::jlimit(1, 16, channel);
    const auto previous = performanceChannel.exchange(channel, std::memory_order_relaxed);
    if (previous == channel) return;
    const auto breathValue = juce::jmax(0, lastBreathMidiValue.load(std::memory_order_relaxed));
    changeKongChannel(previous, channel, breathValue, lastPitchWheel.load(std::memory_order_relaxed),
        [this](int note) { return activeNotes[static_cast<size_t>(note)].load(std::memory_order_relaxed); },
        [this](const auto& message) { queue(message, 0.0); },
        [this](int note) { return activeVelocities[static_cast<size_t>(note)].load(std::memory_order_relaxed); },
        kongVibratoCC.load()==1 ? juce::roundToInt(kongTechniqueInputs[static_cast<size_t>(PerformanceTechnique::vibrato)].load()*127.f) : -1);
}

void PluginHostEngine::configureKongTechniqueProfile(const juce::String& key,const KongTechniqueProfile& profile) noexcept
{
    const auto state=key.isEmpty()?juce::MemoryBlock():captureContainerState();
    const auto tree=KongProjectFile::readPluginComponentState(state);
    const juce::SpinLock::ScopedLockType eventLock(performanceEventLock);
    kongProfile=profile;
    suonaDualVoice.configure(false);
    kongVibratoCC.store(-1);
    for (auto& route : kongTechniqueRoutes) route = {};
    for (auto& value : kongTechniqueInputs) value.store(0.0f, std::memory_order_relaxed);
    switchPerformanceChannel(1);
    if (key.isEmpty()) return;
    player.beginOnsetTrace();
    destinationTrace.begin();
    juce::Logger::writeToLog("ONSET begin 30s bounded capture: " + key);
    if (!tree.isValid() || tree.getChildWithName("PresetList").getNumChildren()==0)
    {
        juce::Logger::writeToLog("Kong technique binding failed: unsupported state; instrument=" + key);
        return;
    }
    if(profile.enabled) {
        // Exact verified two-slot articulation pair only; other/custom suona
        // packages keep established routing. Environment switch is diagnostic
        // rollback, not a new user-facing configuration requirement.
        const auto slots=tree.getChildWithName("PresetList");
        bool main=false,flutter=false;
        for(auto slot:slots){const auto text=slot.toXmlString();
            main|=int(slot.getProperty("MIDI",0))==0 && text.contains("Suona2_Main_Layers");
            flutter|=int(slot.getProperty("MIDI",0))==1 && text.contains("Suona2_Flutter");}
        suonaDualVoice.configure((key=="kong-suona" || key=="kong-suona-2") && slots.getNumChildren()==2 && profile.otherChannel==2 && main && flutter
            && juce::SystemStats::getEnvironmentVariable("FENGYIN_SUONA_LEGACY","0")!="1");
        if (key=="kong-suona" || key=="kong-suona-2")
        {
            juce::Logger::writeToLog("Suona dual route=" + juce::String(suonaDualVoice.active() ? "enabled" : "legacy")
                + " slots=" + juce::String(slots.getNumChildren()) + " main=" + juce::String(main ? 1 : 0)
                + " flutter=" + juce::String(flutter ? 1 : 0) + " otherChannel=" + juce::String(profile.otherChannel));
            for (auto slot : slots)
                juce::Logger::writeToLog("Suona slot ch=" + juce::String(int(slot.getProperty("MIDI",0))+1)
                    + " preset=" + slot.getProperty("Preset").toString());
        }
        if(profile.vibratoCC==1) {
            kongVibratoCC.store(1);
            for(auto slot:tree.getChildWithName("PresetList")) {
                const int channel=int(slot.getProperty("MIDI",0))+1;
                if(channel>=1&&channel<=16)queue(juce::MidiMessage::controllerEvent(channel,1,0),0.0);
            }
        }
        const auto target=simpleOther(key.toStdString());
        if(target!=PerformanceTechnique::count && profile.otherChannel>0) {
            const auto binding=findKongTechniqueAt(tree,profile.otherChannel,profile.otherKeyswitchIndex);
            kongTechniqueRoutes[static_cast<size_t>(target)]={binding.channel,binding.keyswitch,binding.normalKey};
            if(binding.channel==0) juce::Logger::writeToLog("Explicit Qin technique binding rejected: "+key);
        }
        return;
    }
    const auto bind = [this, &tree](PerformanceTechnique technique, const juce::StringArray& names)
    {
        const auto binding = findKongTechnique(tree, names);
        kongTechniqueRoutes[static_cast<size_t>(technique)] = {binding.channel, binding.keyswitch, binding.normalKey};
    };
    // Older/custom Qin packages have no explicit route metadata. Their
    // continuous vibrato uses the same native CC1 as the verified new packs,
    // never a Vib_Main/Vib_Mellow sample keyswitch and held-note restart.
    if(simpleVibrato(key.toStdString())) {
        kongVibratoCC.store(1);
        for(auto slot:tree.getChildWithName("PresetList")) {
            const int channel=int(slot.getProperty("MIDI",0))+1;
            if(channel>=1&&channel<=16)queue(juce::MidiMessage::controllerEvent(channel,1,0),0.0);
        }
    }
    bind(PerformanceTechnique::flutter, {"Flutter", "Flutter2"});
    bind(PerformanceTechnique::tremolo, {"Main_Shake_Mellow", "Roll", "Sus_Main_Roll", "Tremolo"});
    bind(PerformanceTechnique::pizzicato, {"Pizz"});
}

void PluginHostEngine::resetPerformance() noexcept
{
    stringTremolo.request(false);
    const juce::SpinLock::ScopedLockType eventLock(performanceEventLock);
    suonaDualVoice.setFlutter(false);
    if(kontaktStandardControls.load())queue(juce::MidiMessage::controllerEvent(1,1,0),0.0);
    if(kongVibratoCC.load()==1) {
        queue(juce::MidiMessage::controllerEvent(1,1,0),0.0);
        if(kongProfile.otherChannel>1 && kongProfile.otherChannel<=16)
            queue(juce::MidiMessage::controllerEvent(kongProfile.otherChannel,1,0),0.0);
    }
    for (auto& value : lastNativeTechniqueMessages) value.store(0, std::memory_order_relaxed);
    lastBreathMidiValue.store(-1, std::memory_order_relaxed);
    player.requestPerformanceReset();
    performanceChannel.store(1, std::memory_order_relaxed);
    lastPitchWheel.store(8192, std::memory_order_relaxed);
    for (auto& note : activeNotes) note.store(false, std::memory_order_relaxed);
    for (size_t index = 0; index < techniqueValues.size(); ++index)
    {
        kongTechniqueInputs[index].store(0.0f, std::memory_order_relaxed);
        if (techniqueValues[index].exchange(0.0f, std::memory_order_relaxed) != 0.0f)
        {
            if (const int route = nativeTechniqueRoutes[index].load())
                queue(juce::MidiMessage::controllerEvent(route >> 8, (route & 255) - 1, 0), 0.0);
            else techniqueDirty[index].store(true, std::memory_order_release);
        }
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
    if(kontaktStandardControls.load()) {
        // Profile identity is set by the loaded Kensa package, never just by
        // the Kontakt plug-in name. #0001 must also have its verified binding.
        if(!KensaBendRange::boundParameter(*plugin))return false;
        const juce::ScopedLock lock(plugin->getCallbackLock());
        pitchBendChanged(0.0f);
        return KensaBendRange::set(*plugin,semitones);
    }
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
    stringTremolo.flush();
    if(tempoRateDirty.exchange(false)) {
        if(kontaktStandardControls.load()) {
            const auto hz=MusicTempoPolicy::forMode(musicTempoMode.load()).saxRate;
            if(!queue(juce::MidiMessage::controllerEvent(1,19,kensaVibratoRateController(hz)),0.0))
                tempoRateDirty.store(true);
        } else if(auto* p=vibratoRateParameter.load()) {
            const auto target=automaticVibratoActive.load()?tempoVibratoRates[MusicTempoPolicy::valid(musicTempoMode.load())]:originalVibratoRate;
            if(target>=0)p->setValueNotifyingHost(target);
        }
    }
    for (size_t index = 0; index < techniqueParameters.size(); ++index)
        if (techniqueDirty[index].exchange(false, std::memory_order_acq_rel))
            if (nativeTechniqueRoutes[index].load() == 0
                || (techniqueValues[index].load() == 0.0f && (index == static_cast<size_t>(PerformanceTechnique::vibrato)
                    || index == static_cast<size_t>(PerformanceTechnique::growl)
                    || index == static_cast<size_t>(PerformanceTechnique::flutter))))
              if (auto* parameter = techniqueParameters[index].load(std::memory_order_acquire))
                parameter->setValueNotifyingHost(techniqueValues[index].load(std::memory_order_relaxed));
}

bool PluginHostEngine::supportsTechnique(PerformanceTechnique technique) const noexcept
{
    if(technique==PerformanceTechnique::tremolo && stringTremolo.available())return true;
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

bool PluginHostEngine::queue(juce::MidiMessage message, double timestampSeconds) noexcept
{
    // Hardware CCs and other senders can change the same destination. Forget
    // its cached technique value before enqueueing so the next mapped value
    // is delivered even if it equals the value sent before the hardware CC.
    if (message.isController())
        invalidateTechniqueController(nativeTechniqueRoutes, lastNativeTechniqueMessages,
            (message.getChannel() << 8) | (message.getControllerNumber() + 1));
    const bool accepted = player.enqueueMidi(message, timestampSeconds);
    if (!accepted)
        for (auto& value : lastNativeTechniqueMessages) value.store(0, std::memory_order_relaxed);
    return accepted;
}
}
