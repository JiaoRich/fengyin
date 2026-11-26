#pragma once
#include "Sf2State.h"
#include "Sf2Voice.h"
#include <atomic>
#include <bitset>

namespace fengyin::sf2
{
class Processor final : public juce::AudioProcessor, private juce::Thread
{
public:
    Processor();
    ~Processor() override;
    const juce::String getName() const override { return "FengYin SF2"; }
    void prepareToPlay(double, int) override;
    void releaseResources() override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    bool isBusesLayoutSupported(const BusesLayout& b) const override {
        return b.getMainInputChannelSet().isDisabled() && b.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
    }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 1.; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    bool hasEditor() const override { return true; }
    juce::AudioProcessorEditor* createEditor() override;
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;
    State snapshot() const;
    State editorSelection() const;
    juce::var availablePresets() const;
    juce::String statusText() const;
    void chooseFile(const juce::File&);
    void choosePreset(int bank, int program);
    void cancelLoad();
private:
    struct Request { juce::File file; State state; uint64_t generation = 0; double rate = 48000.; };
    struct Candidate { std::unique_ptr<Voice> voice; State state; juce::var presets; uint64_t generation = 0; double rate = 48000.; };
    void request(Request);
    void run() override;
    void reclaimRetired();
    // Callback transfers pointers only. Loader allocates/parses/hashes/frees.
    std::atomic<Candidate*> active {nullptr}, pending {nullptr}, retired {nullptr};
    std::atomic<uint64_t> generation {0};
    std::atomic<double> rate {48000.};
    mutable juce::CriticalSection metadataLock;
    Request next;
    bool requested = false, loading = false, awaitingPreset = false;
    juce::String status, lastError;
    juce::var presets;
    State selection;
    juce::AudioParameterFloat* gain = nullptr;
    juce::AudioParameterInt* bend = nullptr;
    std::bitset<128> notes;
    int expression = 0;
    std::atomic<bool> resetRequested {false};
    juce::SmoothedValue<float> outputGain {1.f};
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Processor)
};
}
