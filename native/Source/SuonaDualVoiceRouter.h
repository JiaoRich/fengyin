#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <atomic>
#include <cmath>

namespace fengyin {
// Only the verified Suona2 ch1/main + ch2/flutter package opts in. No new
// musical NoteOn is generated on a technique edge. Disable restores old routing.
class SuonaDualVoiceRouter {
public:
    SuonaDualVoiceRouter() { scratch.ensureSize(256*1024); }
    void configure(bool on) noexcept { enabled.store(on); reset.store(true); target.store(false); }
    bool active() const noexcept { return enabled.load(); }
    void setFlutter(bool on) noexcept { target.store(on); }
    void process(juce::MidiBuffer& midi, int samples, double sampleRate) {
        if (!enabled.load()) return;
        if (reset.exchange(false)) { position=0; expression=0; lastMain=lastFlutter=-1; }
        scratch.clear();
        for (const auto meta : midi) {
            const auto m=meta.getMessage();
            if (m.getChannel()==1 && m.isController() && m.getControllerNumber()==11) {
                expression=m.getControllerValue();
                writeExpression(meta.samplePosition);
            } else {
                scratch.addEvent(m,meta.samplePosition);
                if (m.getChannel()==1 && (m.isNoteOnOrOff() || m.isPitchWheel() || m.isAftertouch()
                    || m.isChannelPressure() || m.isController())) {
                    auto second=m;second.setChannel(2);scratch.addEvent(second,meta.samplePosition);
                }
            }
        }
        const float step=static_cast<float>(samples/(juce::jmax(1.0,sampleRate)*.040));
        position=juce::jlimit(0.f,1.f,position+(target.load()?step:-step));
        writeExpression(juce::jmax(0,samples-1));
        midi.swapWith(scratch);
    }
private:
    void writeExpression(int sample) {
        const int main=juce::roundToInt(static_cast<float>(expression)*std::sqrt(1.f-position));
        const int flutter=juce::roundToInt(static_cast<float>(expression)*std::sqrt(position));
        if(main!=lastMain){scratch.addEvent(juce::MidiMessage::controllerEvent(1,11,main),sample);lastMain=main;}
        if(flutter!=lastFlutter){scratch.addEvent(juce::MidiMessage::controllerEvent(2,11,flutter),sample);lastFlutter=flutter;}
    }
    std::atomic<bool> enabled{false},target{false},reset{true};
    float position=0;int expression=0,lastMain=-1,lastFlutter=-1;
    juce::MidiBuffer scratch;
};
}
