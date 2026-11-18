#include "SwamTechniqueParameterPolicy.h"
#include "PluginHostEngine.h"
#include "PerformanceReverb.h"
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
#include "KongPerformancePolicy.h"
#include "BendRangeParameters.h"
#include "PerformanceReset.h"
#include "TechniqueProcessingGraph.h"
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
    // Legacy scalar fallback cannot safely reconstruct a plugin MIDI table.
    // Complete native state carries the tuner's mappings and curves instead.
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

bool RecordingAudioProcessorPlayer::enqueueControllerPair(int route,int value,double timestamp) noexcept
{
    const auto status=static_cast<uint8_t>(0xb0|(DjMidiRoute::channel(route)-1));
    const uint8_t bytes[]{status,static_cast<uint8_t>(DjMidiRoute::coarse(route)),static_cast<uint8_t>(value>>7),
                          status,static_cast<uint8_t>(DjMidiRoute::fine(route)),static_cast<uint8_t>(value&127)};
    if(midiQueue.pushPair(bytes,timestamp>0?timestamp:juce::Time::getMillisecondCounterHiRes()*.001,
                          resetGeneration.load(std::memory_order_acquire)))return true;
    requestPerformanceReset();return false;
}

bool RecordingAudioProcessorPlayer::enqueueMidi(const juce::MidiMessage& message,
                                                double timestampSeconds) noexcept
{
    const auto size = message.getRawDataSize();
    if (size <= 0 || size > 3)
        return false;

    const auto timestamp = timestampSeconds > 0.0
        ? timestampSeconds : juce::Time::getMillisecondCounterHiRes() * 0.001;
    if (! midiQueue.push(message.getRawData(), size, timestamp, resetGeneration.load(std::memory_order_acquire)))
    {
        requestPerformanceReset();
        return false;
    }
    return true;
}

void RecordingAudioProcessorPlayer::preparePerformanceMidi(juce::MidiBuffer& messages)
{
    if (deliveredResetGeneration == blockGeneration) return;
    resetMidi.clear();
    sendPerformanceReset(kongResetMode.load(std::memory_order_acquire),
        [this](const juce::MidiMessage& message) { resetMidi.addEvent(message, 0); });
    // Explicit sample-zero ordering: fresh notes/bends at the same sample follow
    // the reset, even when their device timestamp predates this audio callback.
    resetMidi.addEvents(messages, 0, -1, 0);
    messages.swapWith(resetMidi);
    deliveredResetGeneration = blockGeneration;
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
    // Prevent subnormal filter/reverb tails from taking the CPU's slow path.
    // Scoped to this callback; does not change plugin parameters or buffering.
    const juce::ScopedNoDenormals noDenormals;
    const auto nowSeconds = juce::Time::getMillisecondCounterHiRes() * 0.001;
    // Reuse the existing monotonic callback clock; no extra trace clock on the normal path.
    callbackCadence.record(static_cast<uint64_t>(nowSeconds * 1.0e9), numSamples);
    auto traceSink=detailedTiming.sink();
    const audioTrace::Session traceSession(timingEnabled?&traceSink:nullptr);
    bool toneProcessedInGraph=instrumentEffectsInGraph.load(std::memory_order_acquire);
    {
        bool processed = false;
        processorChangeGate.processIfAvailable([&]{
            auto* processor = getCurrentProcessor();
            if (processor == nullptr) return;
            // Hold the same lock used by ScopedGraphPause through the base
            // player's suspension check. Never wait on UI/plugin state work.
            const juce::ScopedTryLock callbackLock(processor->getCallbackLock());
            if (! callbackLock.isLocked() || processor->isSuspended()) return;
            audioTrace::Scope midiTime(audioTrace::Event::midiDrain);
            blockGeneration = resetGeneration.load(std::memory_order_acquire);
            if (blockGeneration != deliveredResetGeneration)
                getMidiMessageCollector().reset(currentSampleRate);
            auto deliver = [&](const RealtimeMidiEvent& event) {
                if (event.generation < blockGeneration) return true;
                if (event.generation > blockGeneration) { deferredMidi = event; return false; }
                for (int offset=0;offset<int(event.size);offset+=3) {
                    juce::MidiMessage message(event.bytes.data()+offset,juce::jmin(3,int(event.size)-offset),event.timestampSeconds);
                    onsetTrace.record(message,event.timestampSeconds);
                    getMidiMessageCollector().addMessageToQueue(message);
                }
                return true;
            };
            bool drain = true;
            if (deferredMidi) {
                const auto event = *deferredMidi;
                deferredMidi.reset();
                drain = deliver(event);
            }
            if (drain) {
                RealtimeMidiEvent event;
                for (std::size_t count = 0; count < midiQueueSize && midiQueue.pop(event); ++count)
                    if (! deliver(event)) break;
            }
            midiTime.finish();
            juce::AudioProcessorPlayer::audioDeviceIOCallbackWithContext(inputs, numInputs, outputs, numOutputs, numSamples, context);
            toneProcessedInGraph=instrumentEffectsInGraph.load(std::memory_order_acquire);
            processed = true;
        });
        if(!processed) {
            if(timingEnabled)changeSkippedBlocks.fetch_add(1,std::memory_order_relaxed);
            for(int channel=0;channel<numOutputs;++channel)
                if(outputs[channel])juce::FloatVectorOperations::clear(outputs[channel],numSamples);
        }
    }
    const auto afterGraph = timingEnabled ? juce::Time::getMillisecondCounterHiRes()*.001 : 0.0;
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
    if (masterOutput != nullptr) {
        if(toneProcessedInGraph) masterOutput->processInstrumentOutput(outputs, numOutputs, numSamples);
        else masterOutput->processInstrument(outputs, numOutputs, numSamples);
    }
    const auto afterInstrument = timingEnabled ? juce::Time::getMillisecondCounterHiRes()*.001 : 0.0;
    if (accompaniment != nullptr)
        accompaniment->mixInto(outputs, numOutputs, numSamples);
    const auto afterAccompaniment = timingEnabled ? juce::Time::getMillisecondCounterHiRes()*.001 : 0.0;
    if (masterOutput != nullptr)
        masterOutput->processMaster(outputs, numOutputs, numSamples);
    const auto afterMaster=timingEnabled?juce::Time::getMillisecondCounterHiRes()*.001:0.0;
    if(timingEnabled){
        const double budget=static_cast<double>(numSamples)/currentSampleRate;
        stageTiming.record(AudioStageTiming::graph,afterGraph-nowSeconds,budget);
        stageTiming.record(AudioStageTiming::instrumentEffects,afterInstrument-afterGraph,budget);
        stageTiming.record(AudioStageTiming::accompaniment,afterAccompaniment-afterInstrument,budget);
        stageTiming.record(AudioStageTiming::master,afterMaster-afterAccompaniment,budget);
    }
    if (recorder != nullptr)
        recorder->push(outputs, numOutputs, numSamples);
    callbackCount.fetch_add(1, std::memory_order_relaxed);
    const auto callbackEnd=juce::Time::getMillisecondCounterHiRes()*.001;
    if(timingEnabled) {
        const double budget=static_cast<double>(numSamples)/currentSampleRate;
        stageTiming.record(AudioStageTiming::callbackTail,callbackEnd-afterMaster,budget);
        stageTiming.record(AudioStageTiming::callbackTotal,callbackEnd-nowSeconds,budget);
    }
    if (callbackEnd - nowSeconds
        > static_cast<double>(numSamples) / currentSampleRate)
        callbackOverruns.fetch_add(1, std::memory_order_relaxed);
}

juce::String PluginHostEngine::takeStageTimingReport()
{
    if(!player.timingEnabled)return {};
    const auto measurements=player.stageTiming.take();
    const char* names[]{"graphAndMidi","instrumentPost","accompaniment","master","callbackTail","callbackTotal"};
    // Message-thread snapshot only. Do not enumerate graph/editor/container
    // ownership on the real-time thread or query third-party plug-in state.
    juce::String result=" hostLifecycle[graphNodes="+juce::String(graph?graph->getNodes().size():0)
        +",serialFx="+juce::String(static_cast<int>(effectNodes.size()))
        +",idleContainers="+juce::String(static_cast<int>(containerCache.size()))
        +",warmingContainers="+juce::String(static_cast<int>(containersWarming.size()))
        +",instrumentEditorRetained="+juce::String(instrumentEditorWindow!=nullptr?1:0)
        +",instrumentEditorVisible="+juce::String(instrumentEditorWindow&&instrumentEditorWindow->isVisible()?1:0)
        +",effectEditorRetained="+juce::String(effectEditorWindow!=nullptr?1:0)
        +",effectEditorVisible="+juce::String(effectEditorWindow&&effectEditorWindow->isVisible()?1:0)
        +",loadGeneration="+juce::String(loadGeneration)+"]";
    for(size_t i=0;i<measurements.size();++i){const auto& m=measurements[i];
        result+=" "+juce::String(names[i])+"[blocks="+juce::String(m.blocks)
            +",meanUs="+juce::String(m.blocks?static_cast<double>(m.totalUs)/m.blocks:0.0,2)
            +",maxUs="+juce::String(m.maxUs)+",over="+juce::String(m.overBudget)+"]";
    }
    const auto details=player.detailedTiming.take();
    result += takeCallbackCadenceReport();
    const char* events[]{"playerLockWait","graphLockWait","nodeLockWait","nodeBodyInclusive",
        "vstLockWait","vstAdapterInclusive","pluginCallWall","techniqueDispatch","midiDrain"};
    const char* roles[]{"host","instrument","serialFx"};
    for(unsigned e=0;e<audioTrace::eventCount;++e)for(unsigned role=0;role<audioTrace::Accumulator::roles;++role) {
        const auto& value=details[e*audioTrace::Accumulator::roles+role];if(!value.calls)continue;
        result+=" "+juce::String(events[e])+"."+roles[role]+"[calls="+juce::String(value.calls)
            +",meanUs="+juce::String(static_cast<double>(value.totalNs)/value.calls/1000.0,2)
            +",maxUs="+juce::String(static_cast<double>(value.maxNs)/1000.0,2)+"]";
    }
    return result;
}

juce::String PluginHostEngine::takeCallbackCadenceReport()
{
    const auto cadence=player.callbackCadence.take();
    return " callbackCadence[epoch="+juce::String(player.callbackCadence.epoch.load())
        +",calls="+juce::String(cadence.calls)+",minFrames="+juce::String(cadence.minFrames)
        +",maxFrames="+juce::String(cadence.maxFrames)+",maxGapUs="
        +juce::String(static_cast<double>(cadence.maxGapNs)/1000.0,2)
        +",changeSkipped="+juce::String(player.changeSkippedBlocks.load())+"]";

}

void RecordingAudioProcessorPlayer::audioDeviceAboutToStart(juce::AudioIODevice* device)
{
    processorChangeGate.change([&] {
        callbackCadence.restart();
        juce::AudioProcessorPlayer::audioDeviceAboutToStart(device);
        getMidiMessageCollector().ensureStorageAllocated(256 * 1024);
        currentSampleRate = device != nullptr && device->getCurrentSampleRate() > 0.0
            ? device->getCurrentSampleRate() : 48000.0;
        if (accompaniment != nullptr && device != nullptr)
            accompaniment->prepare(device->getCurrentSampleRate(), device->getCurrentBufferSizeSamples());
        if (masterOutput != nullptr && device != nullptr)
            masterOutput->setSampleRate(device->getCurrentSampleRate());
    });
}

void RecordingAudioProcessorPlayer::audioDeviceStopped()
{
    processorChangeGate.change([&] {
        if (accompaniment != nullptr)
            accompaniment->release();
        juce::AudioProcessorPlayer::audioDeviceStopped();
    });
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
            processingGraph->preparePerformanceMidi = [this](juce::MidiBuffer& messages) { player.preparePerformanceMidi(messages); };
            processingGraph->applyTechniqueValues = [this] { flushTechniqueValues(); };
            processingGraph->transformPerformanceMidi = [this](juce::MidiBuffer& messages,int count,double rate) {
                juce::ignoreUnused(count);
                const double now = juce::Time::getMillisecondCounterHiRes() * .001;
                for (const auto event : messages)
                    destinationTrace.record(event.getMessage(), now + event.samplePosition / juce::jmax(1.0,rate));
            };
            processingGraph->suppressTechniqueMidi = [this](const juce::MidiMessage& message)
            {
                if (usesDjSource() || kongExpressionMode.load() || kontaktStandardControls.load() || !message.isController()) return false;
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
            player.detailedTiming.instrument.store(instrumentNode->getProcessor());
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
            nativePressureEnabled.store(fengyin::SupportedInstrumentClassifier::classify(
                description.name, description.manufacturerName, description.fileOrIdentifier)
                    == fengyin::SupportedInstrumentClassifier::Brand::swam);
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
    if(instrumentToneNode)graph->removeNode(instrumentToneNode->nodeID);
    instrumentToneNode=nullptr;performanceReverbMix=nullptr;
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
    player.detailedTiming.instrument.store(nullptr);
    // Stop audio before destroying third-party editors and their processors.
    instrumentEditorWindow.reset();
    effectEditorWindow.reset();
    effectNodes.clear();
    djTrackNode=nullptr;djBusNode=nullptr;djOutputNode=nullptr;djSource={};djSourceActive.store(false);djBreathRoute.store(0);
    instrumentToneNode=nullptr;performanceReverbMix=nullptr;performanceReverbEnabled=false;
    player.instrumentEffectsInGraph.store(false,std::memory_order_release);
    instrumentNode = nullptr;
    audioOutputNode = nullptr;
    midiInputNode = nullptr;
    graph.reset();
    currentDescription = {};
    {
        const juce::SpinLock::ScopedLockType eventLock(performanceEventLock);
        kongDualSlot.configure(false);
    }
    kontaktStandardControls.store(false);
    nativePressureEnabled.store(false);
    nativePortamentoInput.store(false);
    stringTremolo.clear();
    for (auto& parameter : techniqueParameters) parameter.store(nullptr);
    for (auto& dirty : techniqueDirty) dirty.store(false, std::memory_order_relaxed);
    instrumentModelParameter = nullptr;
    instrumentModelNames.clear();
    instrumentModelValues.clear();
    swamBreathGrowl.store(false);
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
    if(usesDjSource())return true; // DJ source Attack is deliberate, including the two soprano exceptions.
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

bool PluginHostEngine::applyStandardSwamExpressionCurve(bool initialiseNewInstrument, bool preferBreathGrowl)
{
    if(usesDjSource())return configureDjSource(djSource) && rebuildConnections();
    // The player may still be attached after replacing an instrument. Never
    // restore opaque plugin state concurrently with its audio callback.
    const ScopedGraphPause pause(graph.get());
    juce::Logger::writeToLog("SWAM expression state update begin");
    for (auto& value : lastNativeTechniqueMessages) value.store(0, std::memory_order_relaxed);
    swamBreathGrowl.store(false);
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
    // Full tuner state remains authoritative except for the two destinations
    // owned by global wind techniques. Old packages bind vibrato/growl to CC11,
    // so raw breath defeats OFF/hardware/tempo modes. Reserve independent
    // controllers for those roles; preserve every existing response curve and
    // all unrelated MIDI routes (especially native portamento/Aftertouch).
    const auto configured=SwamTechniqueRouting::prepare(state,simpleVibrato(key),simpleGrowl(key),
        initialiseNewInstrument&&wantFlutter,initialiseNewInstrument,initialiseNewInstrument&&ownTremolo,!initialiseNewInstrument,preferBreathGrowl);
    if(configured.supported && configured.changed)
    {
        // Once per activation, while the graph is paused, never on each CC or mode switch.
        plugin->setStateInformation(state.getData(),static_cast<int>(state.getSize()));
        juce::MemoryBlock verified;plugin->getStateInformation(verified);
        auto check=verified;
        const auto routing=SwamTechniqueRouting::prepare(check,simpleVibrato(key),simpleGrowl(key),
            initialiseNewInstrument&&wantFlutter,initialiseNewInstrument,initialiseNewInstrument&&ownTremolo,!initialiseNewInstrument,preferBreathGrowl);
        if(!routing.supported || routing.changed || routing.breathGrowl!=configured.breathGrowl || routing.vibrato!=configured.vibrato || routing.growl!=configured.growl || routing.flutter!=configured.flutter || routing.volume!=configured.volume)
        {
            plugin->setStateInformation(original.getData(),static_cast<int>(original.getSize()));
            state=original;
            juce::Logger::writeToLog("SWAM automatic technique routing failed verification; original state restored");
        }
        else {state=verified;juce::Logger::writeToLog("SWAM automatic technique routes verified; curves preserved");}
    }
    if (configured.breathGrowl)
    {
        const auto growl = static_cast<size_t>(PerformanceTechnique::growl);
        techniqueValues[growl].store(0.0f, std::memory_order_relaxed);
        techniqueDirty[growl].store(false, std::memory_order_relaxed);
        swamBreathGrowl.store(true);
    }
    const auto mappingState = SwamStateCompatibility::read(state);
    nativePortamentoInput.store(mappingState && SwamTechniqueParameterPolicy::hasNativePortamentoInput(*mappingState));
    const auto result = SwamExpressionCurve::applyToState(state);
    for (const auto& route : result.routes)
    {
        const auto parameter = route.parameter.toLowerCase().removeCharacters(" ._-");
        if (parameter == "expression") swamExpressionChannel.store(route.channel);
        const std::pair<const char*, PerformanceTechnique> names[] = {
            {"vibratodepth", PerformanceTechnique::vibrato}, {"growl", PerformanceTechnique::growl},
            {"flutter", PerformanceTechnique::flutter}, {"portamentotime", PerformanceTechnique::portamento},
            {"tremolo", PerformanceTechnique::tremolo}, {"mute", PerformanceTechnique::mute},
            {"halfvalve", PerformanceTechnique::halfValve}, {"pizzicato", PerformanceTechnique::pizzicato},
            {"breathnoise", PerformanceTechnique::breathNoise}, {"bowpressure", PerformanceTechnique::bowPressure},
            {"overblow", PerformanceTechnique::overblow}, {"fall", PerformanceTechnique::fall},
            {"alternatefingering", PerformanceTechnique::alternateFingering}, {"legato", PerformanceTechnique::legato}
        };
        for (const auto& name : names)
            if (parameter == name.first && !(name.second == PerformanceTechnique::growl && usesSwamBreathGrowl()))
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
    // A restored patch may contain a nonzero native depth even though the global
    // mode is OFF. Clear the performance gate before the first new MIDI phrase;
    // the normal global policy then supplies automatic or hardware depth.
    for (const auto technique : {PerformanceTechnique::vibrato, PerformanceTechnique::growl})
        techniqueChanged(technique, 0.0f);
    if(initialiseNewInstrument)techniqueChanged(PerformanceTechnique::flutter,0.0f);
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
            // New inserts belong before the preserved source return bus.
            const auto insertAt = (usesDjSource() || performanceReverbEnabled) && !effectNodes.empty() ? effectNodes.size()-1 : effectNodes.size();
            effectNodes.insert(effectNodes.begin()+static_cast<std::ptrdiff_t>(insertAt), { effectNode, description });
            if (! rebuildConnections())
            {
                graph->removeNode(effectNode->nodeID);
                effectNodes.erase(effectNodes.begin()+static_cast<std::ptrdiff_t>(insertAt));
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
    if (usesDjSource() || performanceReverbEnabled) return; // Keep the performance reverb slot.
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
        stringTremolo.captureWithoutPerformanceOverride([&] { plugin->getStateInformation(state); });
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
        row->setProperty("fixedBus", (usesDjSource() || performanceReverbEnabled) && &effect == &effectNodes.back());
        result.add(juce::var(row));
    }
    return result;
}

bool PluginHostEngine::removeEffectAt(int index)
{
    if (index < 0 || static_cast<size_t>(index) >= effectNodes.size() || ! graph) return false;
    if ((usesDjSource() || performanceReverbEnabled) && static_cast<size_t>(index)+1 == effectNodes.size()) return false;
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
    if ((usesDjSource() || performanceReverbEnabled) && (static_cast<size_t>(index)+1 == effectNodes.size()
        || static_cast<size_t>(destination)+1 == effectNodes.size())) return false;
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

bool PluginHostEngine::sendDjController(int route,float value,double timestamp) noexcept
{
    if(!route || !std::isfinite(value))return false;
    value=juce::jlimit(0.f,1.f,value);
    if(DjMidiRoute::fine(route)>=0)return player.enqueueControllerPair(route,juce::roundToInt(value*16383.f),timestamp);
    return queue(juce::MidiMessage::controllerEvent(DjMidiRoute::channel(route),DjMidiRoute::coarse(route),juce::roundToInt(value*127.f)),timestamp);
}

bool PluginHostEngine::configureDjSource(const DjSourceProfile& profile)
{
    if(!graph || !getPlugin())return false;
    const ScopedGraphPause pause(graph.get());
    djSource=profile;djSourceActive.store(profile.enabled);
    if(!profile.enabled)return true;
    juce::MemoryBlock state;getPlugin()->getStateInformation(state);
    const auto root=SwamStateCompatibility::read(state);
    if(!root){djSourceActive.store(false);return false;}
    const int expression=DjMidiRoute::find(*root,"expression");
    if(!expression){djSourceActive.store(false);return false;}
    djBreathRoute.store(expression);
    swamExpressionController.store(DjMidiRoute::coarse(expression));swamExpressionChannel.store(DjMidiRoute::channel(expression));
    for(auto& r:nativeTechniqueRoutes)r.store(0);
    const std::pair<const char*,PerformanceTechnique> names[]{
        {"vibratoDepth",PerformanceTechnique::vibrato},{"growl",PerformanceTechnique::growl},
        {"flutter",PerformanceTechnique::flutter},{"portamentoTime",PerformanceTechnique::portamento},
        {"tremolo",PerformanceTechnique::tremolo},{"mute",PerformanceTechnique::mute},
        {"halfValve",PerformanceTechnique::halfValve},{"pizzicato",PerformanceTechnique::pizzicato},
        {"bowPressure",PerformanceTechnique::bowPressure},{"overBlow",PerformanceTechnique::overblow},
        {"fallDown",PerformanceTechnique::fall},{"alternateFingering",PerformanceTechnique::alternateFingering},
        {"legato",PerformanceTechnique::legato}};
    for(const auto& [name,role]:names)nativeTechniqueRoutes[static_cast<size_t>(role)].store(DjMidiRoute::find(*root,name));
    swamBreathGrowl.store(false);stringTremolo.clear();lastBreathMidiValue.store(-1);
    for(auto& dirty:techniqueDirty)dirty.store(false);
    prepareTempoVibrato(*getPlugin());
    if(djTrackNode)graph->removeNode(djTrackNode->nodeID);
    if(djOutputNode)graph->removeNode(djOutputNode->nodeID);
    if(djBusNode)graph->removeNode(djBusNode->nodeID);
    djTrackNode=graph->addNode(std::make_unique<DjFixedGainProcessor>(profile.trackGain));
    djOutputNode=graph->addNode(std::make_unique<DjFixedGainProcessor>(profile.hardwareGain));
    djBusNode=graph->addNode(std::make_unique<DjFixedGainProcessor>(1.f));
    // Restore/route only: never modify the source state, Attack, or mapping curves.
    return true;
}

void PluginHostEngine::configurePerformanceReverb(bool enabled)
{
    const ScopedGraphPause pause(graph.get());
    performanceReverbEnabled=enabled;
    performanceReverbMix=nullptr;
    player.instrumentEffectsInGraph.store(false,std::memory_order_release);
}

bool PluginHostEngine::performanceReverbAvailable() const noexcept
{
    return performanceReverbEnabled && performanceReverbMix && !effectNodes.empty()
        && !effectNodes.back().node->isBypassed();
}
float PluginHostEngine::getPerformanceReverbMix() const noexcept
{
    return performanceReverbMix ? juce::jlimit(0.f,1.f,performanceReverbMix->getValue()) : 0.f;
}
bool PluginHostEngine::setPerformanceReverbMix(float value)
{
    if(!performanceReverbEnabled || !performanceReverbMix || !std::isfinite(value))return false;
    performanceReverbMix->beginChangeGesture();
    performanceReverbMix->setValueNotifyingHost(juce::jlimit(0.f,1.f,value));
    performanceReverbMix->endChangeGesture();
    return true;
}

bool PluginHostEngine::rebuildConnections()
{
    if (graph == nullptr || instrumentNode == nullptr || audioOutputNode == nullptr || midiInputNode == nullptr) return false;
    performanceReverbMix=nullptr;
    player.instrumentEffectsInGraph.store(false,std::memory_order_release);
    if(performanceReverbEnabled) {
        if(effectNodes.empty() || effectNodes.back().description.manufacturerName!="FabFilter"
            || !effectNodes.back().description.name.containsIgnoreCase("Pro-R 2")) return false;
        performanceReverbMix=proR2MixParameter(*effectNodes.back().node->getProcessor());
        if(!performanceReverbMix) return false;
        if(!usesDjSource() && !instrumentToneNode) {
            if(!masterOutputService)return false;
            instrumentToneNode=graph->addNode(std::make_unique<InstrumentToneProcessor>(*masterOutputService));
        }
    }
    for (const auto& connection : graph->getConnections())
        graph->removeConnection(connection);
    const auto midiConnected = graph->addConnection(
        { { midiInputNode->nodeID, juce::AudioProcessorGraph::midiChannelIndex },
          { instrumentNode->nodeID, juce::AudioProcessorGraph::midiChannelIndex } });
    if(usesDjSource()) {
        if(!djTrackNode||!djBusNode||!djOutputNode||effectNodes.empty())return false;
        // Legacy source packages preserve their parallel master send. The new
        // controllable insert owns dry/wet once, so it must not duplicate dry audio.
        const auto& bus=effectNodes.back();
        if(!bus.description.name.containsIgnoreCase("Pro-R 2"))return false;
        std::vector<juce::AudioProcessorGraph::Node::Ptr> pre;
        for(size_t i=0;i+1<effectNodes.size();++i)pre.push_back(effectNodes[i].node);
        return midiConnected && connectDjSourceGraph(*graph,instrumentNode,pre,djTrackNode,
            bus.node,djBusNode,djOutputNode,audioOutputNode,
            djSource.directToMaster && !performanceReverbEnabled);
    }
    std::vector<juce::AudioProcessorGraph::Node::Ptr> chain;
    for (const auto& slot : effectNodes) {
        if(performanceReverbEnabled && &slot==&effectNodes.back())chain.push_back(instrumentToneNode);
        chain.push_back(slot.node);
    }
    player.instrumentEffectsInGraph.store(performanceReverbEnabled,std::memory_order_release);
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
        queueKongPerformance(kongBreathExpression(performanceChannel.load(std::memory_order_relaxed),
            lastBreathMidiValue.load(std::memory_order_relaxed)), timestampSeconds);
        if(kongVibratoCC.load()==1)
            queueKongPerformance(juce::MidiMessage::controllerEvent(performanceChannel.load(),1,
                juce::roundToInt(kongTechniqueInputs[static_cast<size_t>(PerformanceTechnique::vibrato)].load()*127.f)),timestampSeconds);
    }
    activeNotes[static_cast<size_t>(note)].store(true, std::memory_order_relaxed);
    activeVelocities[static_cast<size_t>(note)].store(juce::jlimit(0.0f, 1.0f, velocity), std::memory_order_relaxed);
    const auto message = juce::MidiMessage::noteOn(performanceChannel.load(std::memory_order_relaxed), note,
                                                 juce::jlimit(0.0f, 1.0f, velocity));
    if (kong) queueKongPerformance(message, timestampSeconds);
    else queue(message, timestampSeconds);
}

void PluginHostEngine::noteOff(int noteNumber, double timestampSeconds) noexcept
{
    const juce::SpinLock::ScopedLockType eventLock(performanceEventLock);
    const auto note = juce::jlimit(0, 127, noteNumber);
    activeNotes[static_cast<size_t>(note)].store(false, std::memory_order_relaxed);
    const auto message = juce::MidiMessage::noteOff(performanceChannel.load(std::memory_order_relaxed), note);
    if (kongExpressionMode.load()) queueKongPerformance(message, timestampSeconds);
    else queue(message, timestampSeconds);
    if (kongExpressionMode.load(std::memory_order_relaxed))
    {
        bool playing = false;
        for (const auto& held : activeNotes) playing |= held.load(std::memory_order_relaxed);
        // End the phrase, not each articulation change. Clear any sampler-held
        // notes in all configured slots without resetting pitch or effect tails.
        if (!playing) queueKongPerformance(juce::MidiMessage::allNotesOff(1), timestampSeconds);
    }
}

void PluginHostEngine::breathChanged(float value, double timestampSeconds) noexcept
{
    const juce::SpinLock::ScopedLockType eventLock(performanceEventLock);
    if(usesDjSource()) {
        const int route=djBreathRoute.load();
        const int raw=juce::roundToInt(juce::jlimit(0.f,1.f,value)*(DjMidiRoute::fine(route)>=0?16383.f:127.f));
        if(lastBreathMidiValue.load()!=raw && sendDjController(route,value,timestampSeconds))lastBreathMidiValue.store(raw);
        return;
    }
    const auto midiValue = juce::jlimit(0, 127, juce::roundToInt(value * 127.0f));
    if (lastBreathMidiValue.exchange(midiValue, std::memory_order_relaxed) == midiValue)
        return;
    // The controller's raw 0..127 value is routed to exactly one expression
    // destination. Sending CC2 and CC11 together can drive two mappings inside
    // SWAM and is the main cause of plateaus and conflicting expression curves.
    const auto channel = performanceChannel.load(std::memory_order_relaxed);
    if (kongExpressionMode.load(std::memory_order_relaxed))
        queueKongPerformance(kongBreathExpression(channel, midiValue), timestampSeconds);
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
    const auto message = juce::MidiMessage::pitchWheel(performanceChannel.load(std::memory_order_relaxed), pitch);
    if (kongExpressionMode.load()) queueKongPerformance(message, timestampSeconds);
    else queue(message, timestampSeconds);
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
    if (technique == PerformanceTechnique::growl && usesSwamBreathGrowl()) return;
    const auto safeValue = juce::jlimit(0.0f, 1.0f, value);
    if(usesDjSource()) {
        techniqueValues[index].store(safeValue);
        if(hasIndependentTechniqueRoute(technique))sendDjController(nativeTechniqueRoutes[index].load(),safeValue,0);
        return;
    }
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
        kongTechniqueInputs[index].store(safeValue, std::memory_order_relaxed);
        if (technique == PerformanceTechnique::vibrato) {
            if (kongVibratoCC.load() == 1)
                queueKongPerformance(juce::MidiMessage::controllerEvent(1, 1,
                    juce::roundToInt(safeValue * 127.f)), 0.0);
        } else if (technique == PerformanceTechnique::flutter) {
            kongDualSlot.setTechnique(safeValue > .5f,
                [this](const auto& message) { queue(message, 0.0); });
        }
        return;
    }
    // The tuner owns an enabled native portamento mapping (including its curve).
    // Original pressure reaches SWAM directly; a second linear parameter write
    // from the same learned technique would overwrite the native mapped result.
    if (technique == PerformanceTechnique::portamento && nativePortamentoInput.load()) return;
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

void PluginHostEngine::configureKongTechniqueProfile(const juce::String& key,const KongTechniqueProfile& profile) noexcept
{
    const auto state = key.isEmpty() ? juce::MemoryBlock() : captureContainerState();
    const auto tree = KongProjectFile::readPluginComponentState(state);
    const juce::SpinLock::ScopedLockType eventLock(performanceEventLock);
    kongDualSlot.reset([this](const auto& message) { queue(message, 0.0); });
    kongDualSlot.configure(false);
    kongProfile = profile;
    kongVibratoCC.store(-1);
    for (auto& value : kongTechniqueInputs) value.store(0.0f, std::memory_order_relaxed);
    performanceChannel.store(1, std::memory_order_relaxed);
    if (key.isEmpty()) return;
    player.beginOnsetTrace();
    destinationTrace.begin();
    const auto slots = tree.getChildWithName("PresetList");
    if (!tree.isValid() || slots.getNumChildren() == 0) {
        juce::Logger::writeToLog("Qin channel routing unavailable: unsupported state; instrument=" + key);
        return;
    }
    int mainSlots = 0, techniqueSlots = 0;
    bool otherChannels = false;
    for (auto slot : slots) {
        const int channel = int(slot.getProperty("MIDI", -1)) + 1;
        if (channel == 1) ++mainSlots;
        else if (channel == 2) ++techniqueSlots;
        else otherChannels = true;
    }
    // One shared CH1/CH2 contract. Never infer a preset, library or keyswitch
    // from an instrument name; restore the tuner's saved selection as-is.
    const bool dual = mainSlots == 1 && techniqueSlots == 1 && !otherChannels;
    kongDualSlot.configure(dual);
    kongProfile.enabled = true;
    kongProfile.otherChannel = dual ? 2 : 0;
    kongProfile.otherKeyswitchIndex = -1;
    // Preserve verified continuous-vibrato capability separately from the gate.
    if ((profile.enabled && profile.vibratoCC == 1)
        || (!profile.enabled && simpleVibrato(key.toStdString()))) {
        kongVibratoCC.store(1);
        for (auto slot : slots) {
            const int channel = int(slot.getProperty("MIDI", -1)) + 1;
            if (channel >= 1 && channel <= 16)
                queue(juce::MidiMessage::controllerEvent(channel, 1, 0), 0.0);
        }
    }
    juce::Logger::writeToLog("Qin CH1 main + CH2 technique: "
        + juce::String(dual ? "enabled" : "requires CH1/CH2 scheme") + " instrument=" + key);
}

void PluginHostEngine::resetPerformance() noexcept
{
    stringTremolo.request(false);
    const juce::SpinLock::ScopedLockType eventLock(performanceEventLock);
    // Start the epoch before enqueueing CH2/technique releases so those messages
    // survive along with new input; older queued notes/bends belong to the old sound.
    player.requestPerformanceReset();
    kongDualSlot.reset([this](const auto& message) { queue(message, 0.0); });
    if(kontaktStandardControls.load())queue(juce::MidiMessage::controllerEvent(1,1,0),0.0);
    if(kongVibratoCC.load()==1) {
        queue(juce::MidiMessage::controllerEvent(1,1,0),0.0);
        if(kongProfile.otherChannel>1 && kongProfile.otherChannel<=16)
            queue(juce::MidiMessage::controllerEvent(kongProfile.otherChannel,1,0),0.0);
    }
    for (auto& value : lastNativeTechniqueMessages) value.store(0, std::memory_order_relaxed);
    lastBreathMidiValue.store(-1, std::memory_order_relaxed);
    performanceChannel.store(1, std::memory_order_relaxed);
    lastPitchWheel.store(8192, std::memory_order_relaxed);
    for (auto& note : activeNotes) note.store(false, std::memory_order_relaxed);
    for (size_t index = 0; index < techniqueValues.size(); ++index)
    {
        kongTechniqueInputs[index].store(0.0f, std::memory_order_relaxed);
        if (techniqueValues[index].exchange(0.0f, std::memory_order_relaxed) != 0.0f)
        {
            if(usesDjSource()) { if(hasIndependentTechniqueRoute(static_cast<PerformanceTechnique>(index)))sendDjController(nativeTechniqueRoutes[index].load(),0,0); }
            else if (const int route = nativeTechniqueRoutes[index].load())
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

int PluginHostEngine::getBendRange() const
{
    auto* plugin = getPlugin();
    if (!plugin) return 0;
    if (kontaktStandardControls.load()) {
        if (auto* p=KensaBendRange::boundParameter(*plugin))
            for (int n=1;n<=4;++n)
                if (std::abs(p->getValue()-*KensaBendRange::valueFor(n))<.0001f) return n;
        return 0;
    }
    for (int semitones=1;semitones<=4;++semitones) {
        bool up=false,down=false,matched=true;
        for (auto* parameter:plugin->getParameters()) {
            if (!parameter) continue;
            const auto role=BendRangeParameters::role(parameter->getName(160));
            if (role==BendRangeParameters::none) continue;
            matched &= BendRangeParameters::displays(parameter->getText(parameter->getValue(),128),semitones);
            up |= role==BendRangeParameters::up || role==BendRangeParameters::both;
            down |= role==BendRangeParameters::down || role==BendRangeParameters::both;
        }
        if (matched && up && down) return semitones;
    }
    return 0; // Custom/asymmetric/unsupported ranges stay untouched.
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
    if(usesDjSource())return 0;
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
    if(usesDjSource())return; // No direct parameter fallback may overwrite a source response curve.
    for (size_t index = 0; index < techniqueParameters.size(); ++index)
        if (techniqueDirty[index].exchange(false, std::memory_order_acq_rel)
            && !(index == static_cast<size_t>(PerformanceTechnique::growl) && usesSwamBreathGrowl()))
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
                 && SwamTechniqueParameterPolicy::isPortamentoTime(parameter->getName(128)))
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

bool PluginHostEngine::queueKongPerformance(const juce::MidiMessage& message, double timestampSeconds) noexcept
{
    bool accepted = true;
    kongDualSlot.route(message, [this, timestampSeconds, &accepted](const auto& output) {
        if (!queue(output, timestampSeconds)) accepted = false;
    });
    if (!accepted) kongDualSlot.configure(kongDualSlot.active());
    return accepted;
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

void fengyin::PluginHostEngine::channelPressureChanged(int value, double timestampSeconds) noexcept
{
    if(nativePressureEnabled.load(std::memory_order_relaxed) && !kongExpressionMode.load() && !kontaktStandardControls.load())
        queue(juce::MidiMessage::channelPressureChange(1,juce::jlimit(0,127,value)),timestampSeconds);
}
