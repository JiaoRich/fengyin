#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <atomic>

namespace fengyin {
// Called under PluginHostEngine::performanceEventLock. CH1 always passes
// unchanged; only the technique gate controls musical notes on CH2.
class KongDualSlotRouter {
public:
    void configure(bool enabled) noexcept {
        configured.store(enabled); held.fill(0); sounding.fill(false);
        technique = false; expression = 0; pitch = 8192; vibrato = -1;
    }
    bool active() const noexcept { return configured.load(); }

    template <typename Send>
    void route(const juce::MidiMessage& message, Send&& send) noexcept {
        send(message);
        if (!active() || message.getChannel() != 1) return;
        const auto copy = [&] { auto second = message; second.setChannel(2); send(second); };
        if (message.isNoteOn()) {
            const auto note = static_cast<size_t>(message.getNoteNumber());
            held[note] = message.getVelocity();
            if (technique) {
                if (sounding[note]) send(juce::MidiMessage::noteOff(2, static_cast<int>(note)));
                copy(); sounding[note] = true;
            }
        } else if (message.isNoteOff()) {
            const auto note = static_cast<size_t>(message.getNoteNumber());
            held[note] = 0;
            if (sounding[note]) { copy(); sounding[note] = false; }
        } else if (message.isAllNotesOff() || message.isAllSoundOff()) {
            copy(); held.fill(0); sounding.fill(false);
        } else if (message.isPitchWheel()) {
            pitch = message.getPitchWheelValue(); copy();
        } else if (message.isController()) {
            const auto controller = message.getControllerNumber();
            if (controller == 11) { expression = message.getControllerValue(); copy(); }
            else if (controller == 1) { vibrato = message.getControllerValue(); copy(); }
        } else if (message.isAftertouch() || message.isChannelPressure()) copy();
    }

    template <typename Send>
    void setTechnique(bool enabled, Send&& send) noexcept {
        if (!active() || technique == enabled) return;
        technique = enabled;
        if (enabled) {
            send(juce::MidiMessage::controllerEvent(2, 11, expression));
            send(juce::MidiMessage::pitchWheel(2, pitch));
            if (vibrato >= 0) send(juce::MidiMessage::controllerEvent(2, 1, vibrato));
            for (size_t note = 0; note < held.size(); ++note)
                if (held[note] != 0) {
                    send(juce::MidiMessage::noteOn(2, static_cast<int>(note), held[note]));
                    sounding[note] = true;
                }
        } else {
            for (size_t note = 0; note < sounding.size(); ++note)
                if (sounding[note]) {
                    send(juce::MidiMessage::noteOff(2, static_cast<int>(note)));
                    sounding[note] = false;
                }
        }
    }

    template <typename Send>
    void reset(Send&& send) noexcept {
        setTechnique(false, send);
        held.fill(0); sounding.fill(false);
        expression = 0; pitch = 8192; vibrato = -1;
    }
private:
    std::atomic<bool> configured { false };
    bool technique = false;
    int expression = 0, pitch = 8192, vibrato = -1;
    std::array<juce::uint8, 128> held {};
    std::array<bool, 128> sounding {};
};
}
