#pragma once
#include "SwamToneApplication.h"

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <functional>
#include <memory>
#include <atomic>
#include <array>
#include <vector>
#include <optional>

#include "MidiPerformanceSink.h"
#include "RecordingService.h"
#include "AccompanimentAudioService.h"
#include "MasterOutputService.h"
#include "RealtimeMidiQueue.h"
#include "OrderedMidiQueue.h"
#include "MidiOnsetTrace.h"
#include "SwamToneProfile.h"
#include "SwamExpressionCurve.h"
#include "ToneParameterValue.h"
#include "EffectChainState.h"

namespace fengyin
{
class RecordingAudioProcessorPlayer final : public juce::AudioProcessorPlayer
{
public:
    void setRecordingService(RecordingService* service) noexcept { recorder = service; }
    void setAccompanimentService(AccompanimentAudioService* service) noexcept { accompaniment = service; }
    void setMasterOutputService(MasterOutputService* service) noexcept { masterOutput = service; }
    void setLatencyProbeActive(bool active) noexcept;
    bool enqueueMidi(const juce::MidiMessage& message, double timestampSeconds) noexcept;
    void beginOnsetTrace() noexcept { onsetTrace.begin(); }
    void flushOnsetTrace() { onsetTrace.flush("audio-collector"); }
    void requestPerformanceReset() noexcept { resetRequested.store(true, std::memory_order_release); }
    void setKongResetMode(bool enabled) noexcept { kongResetMode.store(enabled, std::memory_order_release); }
    [[nodiscard]] uint64_t getDroppedMidiEventCount() const noexcept { return midiQueue.droppedCount(); }
    std::atomic<uint64_t> callbackCount { 0 }, callbackOverruns { 0 }, signalBlocks { 0 };
    std::atomic<float> diagnosticSourcePeak { 0 };
    void audioDeviceIOCallbackWithContext(const float* const* inputs, int numInputs, float* const* outputs,
                                          int numOutputs, int numSamples,
                                          const juce::AudioIODeviceCallbackContext& context) override;
    void audioDeviceAboutToStart(juce::AudioIODevice* device) override;
    void audioDeviceStopped() override;
private:
    static constexpr std::size_t midiQueueSize = 8192;
    MidiOnsetTrace onsetTrace;
    void addResetMessages(double timestampSeconds);
    void addLatencyProbeMessages(int numSamples, double timestampSeconds);

    OrderedMidiQueue<midiQueueSize> midiQueue;
    std::atomic<bool> resetRequested { false };
    std::atomic<bool> kongResetMode { false };
    std::atomic<bool> latencyProbeActive { false };
    int probeSamplesUntilChange = 0;
    bool probeNoteIsOn = false;
    int probeNoteIndex = 0;
    int probeCurrentNote = 67;
    double currentSampleRate = 48000.0;
    RecordingService* recorder = nullptr;
    AccompanimentAudioService* accompaniment = nullptr;
    MasterOutputService* masterOutput = nullptr;
};

class PluginHostEngine final : public MidiPerformanceSink
{
public:
    using LoadCallback = std::function<void(bool success, const juce::String& message)>;

    PluginHostEngine();
    ~PluginHostEngine() override;

    void attachTo(juce::AudioDeviceManager& deviceManager);
    void detach();
    void loadAsync(const juce::PluginDescription& description,
                   double sampleRate,
                   int bufferSize,
                   LoadCallback callback);
    void loadAsync(const juce::PluginDescription& description,
                   double sampleRate,
                   int bufferSize,
                   const juce::MemoryBlock& initialState,
                   LoadCallback callback);
    void unload();
    void loadEffectAsync(const juce::PluginDescription& description,
                         double sampleRate, int bufferSize, LoadCallback callback);
    void unloadEffect();
    void setRecordingService(RecordingService* service) noexcept { player.setRecordingService(service); }
    void setAccompanimentService(AccompanimentAudioService* service) noexcept { player.setAccompanimentService(service); }
    void setMasterOutputService(MasterOutputService* service) noexcept { player.setMasterOutputService(service); }
    void setLatencyProbeActive(bool active) noexcept { player.setLatencyProbeActive(active); }
    [[nodiscard]] uint64_t getDroppedMidiEventCount() const noexcept { return player.getDroppedMidiEventCount(); }
    [[nodiscard]] uint64_t getCallbackCount() const noexcept { return player.callbackCount.load(); }
    [[nodiscard]] uint64_t getCallbackOverruns() const noexcept { return player.callbackOverruns.load(); }
    [[nodiscard]] uint64_t getSignalBlocks() const noexcept { return player.signalBlocks.load(); }
    float takeDiagnosticSourcePeak() noexcept { return player.diagnosticSourcePeak.exchange(0); }

    [[nodiscard]] bool hasPlugin() const noexcept;
    [[nodiscard]] juce::String getPluginName() const;
    [[nodiscard]] juce::String getPluginIdentifier() const;
    [[nodiscard]] juce::AudioPluginInstance* getPlugin() const noexcept;
    [[nodiscard]] bool hasEffect() const noexcept { return ! effectNodes.empty(); }
    [[nodiscard]] EffectChainStates captureEffectChain();
    [[nodiscard]] juce::var describeEffectChain() const;
    void restoreEffectChain(const EffectChainStates&, double, int, LoadCallback);
    bool removeEffectAt(int index);
    bool moveEffect(int index, int destination);
    bool setEffectBypassedAt(int index, bool bypassed);
    bool showEffectEditor(int index);
    [[nodiscard]] juce::MemoryBlock captureInstrumentState();
    [[nodiscard]] juce::String getInstrumentDescriptionXml() const;
    [[nodiscard]] juce::String getEffectName() const;
    [[nodiscard]] juce::String getEffectIdentifier() const;
    [[nodiscard]] juce::MemoryBlock saveEffectState() const;
    bool restoreEffectState(const void* data, std::size_t size);
    bool setEffectBypassed(bool shouldBypass);
    [[nodiscard]] bool isEffectBypassed() const noexcept;
    bool showPluginEditor(bool effect);
    void closePluginEditor(bool effect = false);
    [[nodiscard]] juce::Array<ToneParameterValue> captureToneParameters() const;
    int restoreToneParameters(const juce::Array<ToneParameterValue>& parameters);
    [[nodiscard]] juce::StringArray getProgramNames() const;
    [[nodiscard]] juce::String getCurrentProgramName() const;
    [[nodiscard]] juce::MemoryBlock captureKongState();
    bool restoreKongProject(const juce::MemoryBlock& project);
    [[nodiscard]] juce::MemoryBlock captureContainerState();
    bool restoreContainerState(const juce::MemoryBlock& state);
    bool selectProgramByAliases(const juce::StringArray& aliases);
    [[nodiscard]] juce::StringArray getInstrumentModelNames() const;
    [[nodiscard]] int getCurrentInstrumentModelIndex() const noexcept;
    [[nodiscard]] juce::String getCurrentInstrumentModelName() const;
    bool selectInstrumentModel(int index);
    int applySwamToneProfile(const SwamToneProfile& profile, bool restoreModel = true);
    [[nodiscard]] juce::var getSwamToneAudit() const { return swamToneAudit; }
    bool applyStandardSwamExpressionCurve();
    bool applyExpressionAttackControl();
    bool setBendRange(int semitones);
    void configureKongTechniqueProfile(const juce::String& instrumentKey) noexcept;
    void flushOnsetTrace() { player.flushOnsetTrace(); }

    void noteOn(int noteNumber, float velocity, double timestampSeconds = 0.0) noexcept override;
    void noteOff(int noteNumber, double timestampSeconds = 0.0) noexcept override;
    void breathChanged(float value, double timestampSeconds = 0.0) noexcept override;
    void setKongExpressionMode(bool enabled) noexcept
    {
        kongExpressionMode.store(enabled);
        player.setKongResetMode(enabled);
    }
    void pitchBendChanged(float bipolarValue, double timestampSeconds = 0.0) noexcept override;
    void techniqueChanged(PerformanceTechnique technique, float value) noexcept override;
    void resetPerformance() noexcept override;
    void flushTechniqueValues();
    [[nodiscard]] bool supportsTechnique(PerformanceTechnique technique) const noexcept;
    [[nodiscard]] int getProcessingLatencySamples() const noexcept;

private:
    juce::var swamToneAudit;
    void queue(juce::MidiMessage message, double timestampSeconds) noexcept;
    bool rebuildConnections();
    void resolveTechniqueParameters();
    void resolveInstrumentModelParameter();

    juce::AudioPluginFormatManager formatManager;
    RecordingAudioProcessorPlayer player;
    std::unique_ptr<juce::AudioProcessorGraph> graph;
    juce::AudioProcessorGraph::Node::Ptr instrumentNode;
    struct EffectSlot
    {
        juce::AudioProcessorGraph::Node::Ptr node;
        juce::PluginDescription description;
    };
    std::vector<EffectSlot> effectNodes;
    juce::AudioProcessorGraph::Node::Ptr audioOutputNode;
    juce::AudioProcessorGraph::Node::Ptr midiInputNode;
    juce::PluginDescription currentDescription;
    juce::Array<ToneParameterValue> releaseBaseline;
    juce::Array<ToneParameterValue> previousReleaseParameters;
    class PluginEditorWindow;
    std::unique_ptr<PluginEditorWindow> instrumentEditorWindow;
    std::unique_ptr<PluginEditorWindow> effectEditorWindow;
    juce::AudioDeviceManager* attachedManager = nullptr;
    std::shared_ptr<std::atomic_bool> lifetime = std::make_shared<std::atomic_bool>(true);
    uint64_t loadGeneration = 0;
    uint64_t effectLoadGeneration = 0;
    std::array<std::atomic<juce::AudioProcessorParameter*>, static_cast<size_t>(PerformanceTechnique::count)> techniqueParameters {};
    std::array<std::atomic<float>, static_cast<size_t>(PerformanceTechnique::count)> techniqueValues {};
    std::array<std::atomic<bool>, static_cast<size_t>(PerformanceTechnique::count)> techniqueDirty {};
    juce::AudioProcessorParameter* instrumentModelParameter = nullptr;
    juce::StringArray instrumentModelNames;
    std::vector<float> instrumentModelValues;
    std::atomic<bool> kongExpressionMode { false };
    std::atomic<int> swamExpressionController { 11 };
    std::atomic<int> lastBreathMidiValue { -1 };
    struct KongTechniqueRoute { int channel = 0; int keyswitch = -1; int normalKey = -1; };
    std::array<KongTechniqueRoute, static_cast<size_t>(PerformanceTechnique::count)> kongTechniqueRoutes {};
    std::array<std::atomic<float>, static_cast<size_t>(PerformanceTechnique::count)> kongTechniqueInputs {};
    std::array<std::atomic<bool>, 128> activeNotes {};
    std::array<std::atomic<float>, 128> activeVelocities {};
    std::atomic<int> performanceChannel { 1 };
    std::atomic<int> lastPitchWheel { 8192 };
    void switchPerformanceChannel(int channel) noexcept;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginHostEngine)
};
}
