#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <functional>
#include <memory>
#include <atomic>
#include <array>

#include "MidiPerformanceSink.h"
#include "RecordingService.h"
#include "AccompanimentAudioService.h"
#include "MasterOutputService.h"

namespace fengyin
{
class RecordingAudioProcessorPlayer final : public juce::AudioProcessorPlayer
{
public:
    void setRecordingService(RecordingService* service) noexcept { recorder = service; }
    void setAccompanimentService(AccompanimentAudioService* service) noexcept { accompaniment = service; }
    void setMasterOutputService(MasterOutputService* service) noexcept { masterOutput = service; }
    void audioDeviceIOCallbackWithContext(const float* const* inputs, int numInputs, float* const* outputs,
                                          int numOutputs, int numSamples,
                                          const juce::AudioIODeviceCallbackContext& context) override;
    void audioDeviceAboutToStart(juce::AudioIODevice* device) override;
    void audioDeviceStopped() override;
private:
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
    void unload();
    void loadEffectAsync(const juce::PluginDescription& description,
                         double sampleRate, int bufferSize, LoadCallback callback);
    void unloadEffect();
    void setRecordingService(RecordingService* service) noexcept { player.setRecordingService(service); }
    void setAccompanimentService(AccompanimentAudioService* service) noexcept { player.setAccompanimentService(service); }
    void setMasterOutputService(MasterOutputService* service) noexcept { player.setMasterOutputService(service); }

    [[nodiscard]] bool hasPlugin() const noexcept;
    [[nodiscard]] juce::String getPluginName() const;
    [[nodiscard]] juce::String getPluginIdentifier() const;
    [[nodiscard]] juce::AudioPluginInstance* getPlugin() const noexcept;
    [[nodiscard]] bool hasEffect() const noexcept { return effectNode != nullptr; }
    [[nodiscard]] juce::String getEffectName() const;
    [[nodiscard]] juce::String getEffectIdentifier() const;
    [[nodiscard]] juce::MemoryBlock saveEffectState() const;
    bool restoreEffectState(const void* data, std::size_t size);
    bool setEffectBypassed(bool shouldBypass);
    [[nodiscard]] bool isEffectBypassed() const noexcept;
    bool showPluginEditor(bool effect);
    [[nodiscard]] juce::MemoryBlock savePluginState() const;
    bool restorePluginState(const void* data, std::size_t size);

    void noteOn(int noteNumber, float velocity) noexcept override;
    void noteOff(int noteNumber) noexcept override;
    void breathChanged(float value) noexcept override;
    void pitchBendChanged(float bipolarValue) noexcept override;
    void techniqueChanged(PerformanceTechnique technique, float value) noexcept override;
    void flushTechniqueValues();
    [[nodiscard]] bool supportsTechnique(PerformanceTechnique technique) const noexcept;

private:
    void queue(juce::MidiMessage message) noexcept;
    bool rebuildConnections();
    void resolveTechniqueParameters();

    juce::AudioPluginFormatManager formatManager;
    RecordingAudioProcessorPlayer player;
    std::unique_ptr<juce::AudioProcessorGraph> graph;
    juce::AudioProcessorGraph::Node::Ptr instrumentNode;
    juce::AudioProcessorGraph::Node::Ptr effectNode;
    juce::AudioProcessorGraph::Node::Ptr audioOutputNode;
    juce::AudioProcessorGraph::Node::Ptr midiInputNode;
    juce::PluginDescription currentDescription;
    juce::PluginDescription currentEffectDescription;
    class PluginEditorWindow;
    std::unique_ptr<PluginEditorWindow> instrumentEditorWindow;
    std::unique_ptr<PluginEditorWindow> effectEditorWindow;
    juce::AudioDeviceManager* attachedManager = nullptr;
    std::shared_ptr<std::atomic_bool> lifetime = std::make_shared<std::atomic_bool>(true);
    std::array<juce::AudioProcessorParameter*, static_cast<size_t>(PerformanceTechnique::count)> techniqueParameters {};
    std::array<std::atomic<float>, static_cast<size_t>(PerformanceTechnique::count)> techniqueValues {};
    std::array<std::atomic<bool>, static_cast<size_t>(PerformanceTechnique::count)> techniqueDirty {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginHostEngine)
};
}
