#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <atomic>

namespace fengyin {
// Called under PluginHostEngine::performanceEventLock. Only one slot receives
// musical notes: CH1 normally, CH2 while the technique key is held.
class KongDualSlotRouter {
public:
    void configure(bool enabled) noexcept {
        configured.store(enabled); held.fill(0);
        technique = false; expression = 0; pitch = 8192; vibrato = -1;
    }
    bool active() const noexcept { return configured.load(); }

    template <typename Send>
    void route(const juce::MidiMessage& message, Send&& send) noexcept {
        if (!active() || message.getChannel() != 1) { send(message); return; }
        const auto copyTo = [&](int channel) { auto copy = message; copy.setChannel(channel); send(copy); };
        if (message.isNoteOn()) {
            const auto note = static_cast<size_t>(message.getNoteNumber());
            if (held[note] != 0) send(juce::MidiMessage::noteOff(technique ? 2 : 1, static_cast<int>(note)));
            held[note] = message.getVelocity();
            copyTo(technique ? 2 : 1);
        } else if (message.isNoteOff()) {
            held[static_cast<size_t>(message.getNoteNumber())] = 0;
            copyTo(technique ? 2 : 1);
        } else if (message.isAllNotesOff() || message.isAllSoundOff()) {
            send(message); copyTo(2); held.fill(0);
        } else {
            send(message);
            if (message.isPitchWheel()) { pitch = message.getPitchWheelValue(); copyTo(2); }
            else if (message.isController()) {
                if (message.getControllerNumber() == 11) { expression = message.getControllerValue(); copyTo(2); }
                else if (message.getControllerNumber() == 1) { vibrato = message.getControllerValue(); copyTo(2); }
            } else if (message.isAftertouch() || message.isChannelPressure()) copyTo(2);
        }
    }

    template <typename Send>
    void setTechnique(bool enabled, Send&& send) noexcept {
        if (!active() || technique == enabled) return;
        const int previous = technique ? 2 : 1, next = enabled ? 2 : 1;
        // Stop outgoing notes before starting the destination. No sustained
        // layering or added gain fades; each slot retains its native release.
        for (size_t note = 0; note < held.size(); ++note)
            if (held[note] != 0) send(juce::MidiMessage::noteOff(previous, static_cast<int>(note)));
        technique = enabled;
        send(juce::MidiMessage::controllerEvent(next, 11, expression));
        send(juce::MidiMessage::pitchWheel(next, pitch));
        if (vibrato >= 0) send(juce::MidiMessage::controllerEvent(next, 1, vibrato));
        for (size_t note = 0; note < held.size(); ++note)
            if (held[note] != 0) send(juce::MidiMessage::noteOn(next, static_cast<int>(note), held[note]));
    }

    template <typename Send>
    void reset(Send&& send) noexcept {
        if (active()) for (size_t note = 0; note < held.size(); ++note)
            if (held[note] != 0) send(juce::MidiMessage::noteOff(technique ? 2 : 1, static_cast<int>(note)));
        held.fill(0); technique = false;
        expression = 0; pitch = 8192; vibrato = -1;
    }
private:
    std::atomic<bool> configured { false };
    bool technique = false;
    int expression = 0, pitch = 8192, vibrato = -1;
    std::array<juce::uint8, 128> held {};
};
}
