#include "KongDualSlotRouter.h"
#include <cassert>
#include <vector>

int main()
{
    using M = juce::MidiMessage;
    fengyin::KongDualSlotRouter router;
    std::vector<M> messages;
    auto send = [&](const M& m) { messages.push_back(m); };
    auto route = [&](const M& m) { router.route(m, send); };
    auto gate = [&](bool enabled) { messages.clear(); router.setTechnique(enabled, send); };
    auto count = [&](int channel, bool on) {
        int n = 0;
        for (const auto& m : messages)
            if (m.getChannel() == channel && (on ? m.isNoteOn() : m.isNoteOff())) ++n;
        return n;
    };
    router.configure(true);
    route(M::controllerEvent(1, 11, 103));
    route(M::controllerEvent(1, 1, 25));
    route(M::pitchWheel(1, 10000));
    route(M::noteOn(1, 60, juce::uint8(91)));
    assert(count(1, true) == 1 && count(2, true) == 0);
    gate(true);
    assert(count(1, true) == 0 && count(1, false) == 0 && count(2, true) == 1);
    assert(messages[0].isControllerOfType(11) && messages[0].getControllerValue() == 103);
    assert(messages[1].isPitchWheel() && messages[1].getPitchWheelValue() == 10000);
    assert(messages[2].isControllerOfType(1) && messages[2].getControllerValue() == 25);
    assert(messages.back().getNoteNumber() == 60 && messages.back().getVelocity() == 91);
    // Turning the key off never changes main expression, notes or articulation.
    gate(false);
    assert(messages.size() == 1 && count(2, false) == 1);
    gate(false); assert(messages.empty());
    gate(true); assert(count(2, true) == 1);
    messages.clear();
    route(M::noteOff(1, 60)); route(M::noteOn(1, 67, juce::uint8(80)));
    assert(count(1, false) == 1 && count(2, false) == 1);
    assert(count(1, true) == 1 && count(2, true) == 1);
    gate(false);
    assert(messages.size() == 1 && messages[0].getNoteNumber() == 67);
    messages.clear(); route(M::noteOff(1, 67));
    assert(messages.size() == 1 && count(1, false) == 1);
    gate(true); assert(count(2, true) == 0); // no stale pitch after stopping
    messages.clear(); route(M::noteOn(1, 72, juce::uint8(88)));
    assert(count(2, true) == 1); // technique held before the next phrase
    messages.clear(); router.reset(send);
    assert(count(2, false) == 1 && count(1, false) == 0);
    gate(true); assert(count(2, true) == 0);
    // AllNotesOff / panic clears both channels and router tracking.
    route(M::noteOn(1, 62, juce::uint8(90)));
    messages.clear(); route(M::allNotesOff(1));
    assert(messages.size() == 2 && messages[1].isAllNotesOff());
    gate(false); assert(messages.empty());
    gate(true); assert(count(2, true) == 0);
    // Repeated on/off cycles leave no second-channel notes outstanding.
    messages.clear(); route(M::noteOn(1, 64, juce::uint8(76)));
    int balance = 1;
    for (int iteration = 0; iteration < 1000; ++iteration) {
        gate(false); balance -= count(2, false);
        gate(true); balance += count(2, true);
        assert(balance == 1);
    }
    gate(false); balance -= count(2, false); assert(balance == 0);
    router.configure(false);
    messages.clear(); route(M::noteOn(1, 67, juce::uint8(90))); gate(true);
    assert(messages.empty());
    // Unconfigured single-slot schemes remain a transparent MIDI pass-through.
    messages.clear(); route(M::controllerEvent(1, 11, 50));
    assert(messages.size() == 1 && messages[0].getControllerValue() == 50);
}
