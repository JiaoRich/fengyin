#pragma once
#include "RealtimeMidiQueue.h"
#include <juce_audio_basics/juce_audio_basics.h>
namespace fengyin {
// One producer per trace; bounded and allocation-free on MIDI/audio threads.
// Disk logging is performed only by the UI timer.
class MidiOnsetTrace {
public:
    // Arm at load, but begin the timed window on the first musical Note On.
    // Users may spend minutes choosing a preset before they start playing.
    void begin() noexcept { remaining.store(2048); endsAt.store(-1.0); }
    void record(const juce::MidiMessage& message, double timestamp) noexcept {
        if (remaining.load()<=0) return;
        const auto now = juce::Time::getMillisecondCounterHiRes()*.001;
        auto end = endsAt.load();
        if (end < 0.0)
        {
            // Exclude low keyswitch notes and idle controller noise while armed.
            if (!message.isNoteOn() || message.getNoteNumber() < 36) return;
            end = now + 30.0;
            endsAt.store(end);
        }
        if (now>end) return;
        if (!message.isNoteOnOrOff() && !message.isController()) return;
        remaining.fetch_sub(1);
        queue.push(message.getRawData(),message.getRawDataSize(),timestamp);
    }
    void flush(const char* stage) {
        RealtimeMidiEvent e;
        for(int n=0;n<512 && queue.pop(e);++n)
            juce::Logger::writeToLog(juce::String("ONSET ")+stage+" t="+juce::String(e.timestampSeconds,6)
                +" status="+juce::String(static_cast<int>(e.bytes[0]))
                +" data1="+juce::String(static_cast<int>(e.bytes[1]))+" data2="+juce::String(static_cast<int>(e.bytes[2])));
    }
private:
    RealtimeMidiQueue<4096> queue;
    std::atomic<int> remaining{0};
    std::atomic<double> endsAt{0};
};
}
