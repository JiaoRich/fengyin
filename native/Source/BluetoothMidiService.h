#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_data_structures/juce_data_structures.h>
#include <functional>
#include <memory>

namespace fengyin
{
// Control-plane work lives on an MTA worker; MIDI delivery never visits the browser.
class BluetoothMidiService final : private juce::AsyncUpdater
{
public:
    explicit BluetoothMidiService(bool persistSettings = true);
    ~BluetoothMidiService() override;
    void command(const juce::String& action, const juce::String& id = {});
    juce::var snapshot() const;
    void setReceiver(std::function<void(const juce::MidiMessage&)>);
    void shutdown();
    std::function<void()> onChange; // Message thread; also called for disconnects.
private:
    void handleAsyncUpdate() override;
    struct Impl;
    std::unique_ptr<Impl> impl;
};
}
