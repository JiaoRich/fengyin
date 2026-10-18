#pragma once
#include <juce_core/juce_core.h>

namespace fengyin {
// Message-thread processor preparation may wait for an in-flight audio block;
// the reverse direction must never wait for plugin preparation/destruction.
class ProcessorChangeGate {
public:
    template<class Function> void change(Function&& function) {
        const juce::ScopedLock lock(section);function();
    }
    template<class Function> bool processIfAvailable(Function&& function) {
        const juce::ScopedTryLock lock(section);
        if(!lock.isLocked())return false;
        function();return true;
    }
private:
    juce::CriticalSection section;
};
}
