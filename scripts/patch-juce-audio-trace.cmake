# Observation only: no lock replacement, try-lock, skipped samples, or buffering
# change. Consumers without FENGYIN_TRACE_AUDIO_LOCKS compile the original path.
set(trace_include "#if FENGYIN_TRACE_AUDIO_LOCKS\n#include <AudioCallbackTrace.h>\n#endif\n")
set(graph "${JUCE_SOURCE_DIR}/modules/juce_audio_processors_headless/processors/juce_AudioProcessorGraph.cpp")
set(vst "${JUCE_SOURCE_DIR}/modules/juce_audio_processors_headless/format_types/juce_VST3PluginFormatImpl.h")
set(player "${JUCE_SOURCE_DIR}/modules/juce_audio_utils/players/juce_AudioProcessorPlayer.cpp")
replace_exact("${graph}" "namespace juce\n{" "${trace_include}\nnamespace juce\n{")
replace_exact("${graph}"
[[            const ScopedLock lock { this->processor.getCallbackLock() };
            callProcess (bypass, audio, midi);]]
[[           #if FENGYIN_TRACE_AUDIO_LOCKS
            fengyin::audioTrace::Scope wait (fengyin::audioTrace::Event::nodeLock, &this->processor);
           #endif
            const ScopedLock lock { this->processor.getCallbackLock() };
           #if FENGYIN_TRACE_AUDIO_LOCKS
            wait.finish();
            fengyin::audioTrace::Scope body (fengyin::audioTrace::Event::nodeBody, &this->processor);
           #endif
            callProcess (bypass, audio, midi);]])
replace_exact("${player}" "namespace juce\n{" "${trace_include}\nnamespace juce\n{")
replace_exact("${player}"
[[    const ScopedLock sl (lock);

    jassert (currentDevice != nullptr);]]
[[   #if FENGYIN_TRACE_AUDIO_LOCKS
    fengyin::audioTrace::Scope playerWait (fengyin::audioTrace::Event::playerLock);
   #endif
    const ScopedLock sl (lock);
   #if FENGYIN_TRACE_AUDIO_LOCKS
    playerWait.finish();
   #endif

    jassert (currentDevice != nullptr);]])
replace_exact("${player}"
[[        const ScopedLock sl2 (processor->getCallbackLock());]]
[[       #if FENGYIN_TRACE_AUDIO_LOCKS
        fengyin::audioTrace::Scope graphWait (fengyin::audioTrace::Event::graphLock);
       #endif
        const ScopedLock sl2 (processor->getCallbackLock());
       #if FENGYIN_TRACE_AUDIO_LOCKS
        graphWait.finish();
       #endif]])
replace_exact("${vst}" "namespace juce\n{" "${trace_include}\nnamespace juce\n{")
replace_exact("${vst}"
[[        const SpinLock::ScopedLockType processLock (processMutex);

        if (isActive && processor != nullptr)
            processAudio (buffer, midiMessages, Vst::kSample32, false);]]
[[       #if FENGYIN_TRACE_AUDIO_LOCKS
        ::fengyin::audioTrace::Scope wait (::fengyin::audioTrace::Event::vstLock, static_cast<AudioProcessor*> (this));
       #endif
        const SpinLock::ScopedLockType processLock (processMutex);
       #if FENGYIN_TRACE_AUDIO_LOCKS
        wait.finish();
        ::fengyin::audioTrace::Scope adapter (::fengyin::audioTrace::Event::vstAdapter, static_cast<AudioProcessor*> (this));
       #endif

        if (isActive && processor != nullptr)
            processAudio (buffer, midiMessages, Vst::kSample32, false);]])
replace_exact("${vst}"
[[        const SpinLock::ScopedLockType processLock (processMutex);

        if (isActive && processor != nullptr)
            processAudio (buffer, midiMessages, Vst::kSample64, false);]]
[[       #if FENGYIN_TRACE_AUDIO_LOCKS
        ::fengyin::audioTrace::Scope wait (::fengyin::audioTrace::Event::vstLock, static_cast<AudioProcessor*> (this));
       #endif
        const SpinLock::ScopedLockType processLock (processMutex);
       #if FENGYIN_TRACE_AUDIO_LOCKS
        wait.finish();
        ::fengyin::audioTrace::Scope adapter (::fengyin::audioTrace::Event::vstAdapter, static_cast<AudioProcessor*> (this));
       #endif

        if (isActive && processor != nullptr)
            processAudio (buffer, midiMessages, Vst::kSample64, false);]])
replace_exact("${vst}" "        processor->process (data);"
[[        {
           #if FENGYIN_TRACE_AUDIO_LOCKS
            ::fengyin::audioTrace::Scope call (::fengyin::audioTrace::Event::pluginCall, static_cast<AudioProcessor*> (this));
           #endif
            processor->process (data);
        }]])
