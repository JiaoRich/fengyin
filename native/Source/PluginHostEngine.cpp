#include "PluginHostEngine.h"

#include <algorithm>
#include <limits>

namespace fengyin
{
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
    juce::AudioProcessorPlayer::audioDeviceIOCallbackWithContext(inputs, numInputs, outputs, numOutputs, numSamples, context);
    if (accompaniment != nullptr)
        accompaniment->mixInto(outputs, numOutputs, numSamples);
    if (masterOutput != nullptr)
        masterOutput->process(outputs, numOutputs, numSamples);
    if (recorder != nullptr)
        recorder->push(outputs, numOutputs, numSamples);
}

void RecordingAudioProcessorPlayer::audioDeviceAboutToStart(juce::AudioIODevice* device)
{
    juce::AudioProcessorPlayer::audioDeviceAboutToStart(device);
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
            graph = std::make_unique<juce::AudioProcessorGraph>();
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
            if (! rebuildConnections())
            {
                unload();
                if (completion)
                    completion(false, juce::String::fromUTF8("音源已打开，但无法连接到声音输出，请重新选择声音设备"));
                return;
            }
            player.setProcessor(graph.get());
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

void PluginHostEngine::noteOn(int noteNumber, float velocity) noexcept
{
    queue(juce::MidiMessage::noteOn(1, juce::jlimit(0, 127, noteNumber),
                                    juce::jlimit(0.0f, 1.0f, velocity)));
}

void PluginHostEngine::noteOff(int noteNumber) noexcept
{
    queue(juce::MidiMessage::noteOff(1, juce::jlimit(0, 127, noteNumber)));
}

void PluginHostEngine::breathChanged(float value) noexcept
{
    const auto midiValue = juce::jlimit(0, 127, juce::roundToInt(value * 127.0f));
    queue(juce::MidiMessage::controllerEvent(1, 11, midiValue));
    queue(juce::MidiMessage::controllerEvent(1, 2, midiValue));
}

void PluginHostEngine::pitchBendChanged(float bipolarValue) noexcept
{
    const auto pitch = juce::jlimit(0, 16383,
        juce::roundToInt(8192.0f + juce::jlimit(-1.0f, 1.0f, bipolarValue) * 8191.0f));
    queue(juce::MidiMessage::pitchWheel(1, pitch));
}

void PluginHostEngine::queue(juce::MidiMessage message) noexcept
{
    player.getMidiMessageCollector().addMessageToQueue(message);
}
}
