from pathlib import Path
import unittest

ROOT=Path(__file__).resolve().parents[1]

class AudioLockTrace(unittest.TestCase):
    def test_hooks_observe_original_blocking_locks_and_plugin_call(self):
        patch=(ROOT/'scripts/patch-juce-audio-trace.cmake').read_text(encoding='utf-8')
        for event in ['nodeLock','nodeBody','playerLock','graphLock','vstLock','vstAdapter','pluginCall']:
            self.assertIn('Event::'+event,patch)
        for original in ['const ScopedLock lock { this->processor.getCallbackLock() };',
                         'const ScopedLock sl (lock);','const ScopedLock sl2 (processor->getCallbackLock());',
                         'const SpinLock::ScopedLockType processLock (processMutex);','processor->process (data);']:
            self.assertIn(original,patch)
        self.assertNotIn('ScopedTryLock',patch)
        self.assertNotIn('suspendProcessing',patch)
        self.assertNotIn('buffer.clear',patch)

    def test_disabled_trace_has_no_clock_or_disk_work(self):
        trace=(ROOT/'native/Source/AudioCallbackTrace.h').read_text(encoding='utf-8')
        self.assertIn('started(sink?now():0)',trace)
        self.assertIn('if(auto* target=sink)',trace)
        for forbidden in ['ofstream','writeToLog','appendText','mutex','new ']:
            self.assertNotIn(forbidden,trace)

    def test_diagnostics_survive_hidden_window_and_name_wall_time(self):
        main=(ROOT/'native/Source/MainComponent.cpp').read_text(encoding='utf-8')
        timer=main.split('void MainComponent::timerCallback()',1)[1]
        self.assertLess(timer.index('if(pluginHost.isStageTimingEnabled())logAudioDiagnostic(currentAudio)'),
                        timer.index('if (webPresentationVisible && webInterface'))
        self.assertIn('audio-timing-diagnostic.log',main)
        host=(ROOT/'native/Source/PluginHostEngine.cpp').read_text(encoding='utf-8')
        self.assertIn('pluginCallWall',host)
        self.assertIn('nodeBodyInclusive',host)
        self.assertIn('vstAdapterInclusive',host)
        self.assertIn('changeSkipped=',host)
        self.assertIn('callbackCadence.restart()',host)

    def test_lifecycle_snapshot_is_opt_in_and_outside_audio_callback(self):
        host=(ROOT/'native/Source/PluginHostEngine.cpp').read_text(encoding='utf-8')
        report=host.split('juce::String PluginHostEngine::takeStageTimingReport()',1)[1].split(
            'void RecordingAudioProcessorPlayer::audioDeviceAboutToStart',1)[0]
        self.assertLess(report.index('if(!player.timingEnabled)return {};'),report.index('hostLifecycle'))
        for field in ['graphNodes','serialFx','idleContainers','warmingContainers',
                      'instrumentEditorRetained','instrumentEditorVisible','effectEditorRetained','effectEditorVisible']:
            self.assertIn(field,report)
        callback=host.split('void RecordingAudioProcessorPlayer::audioDeviceIOCallbackWithContext',1)[1].split(
            'juce::String PluginHostEngine::takeStageTimingReport()',1)[0]
        for forbidden in ['getNodes()', 'isVisible()', 'hostLifecycle']:
            self.assertNotIn(forbidden,callback)
        for forbidden in ['getStateInformation','captureToneParameters','setValueNotifyingHost']:
            self.assertNotIn(forbidden,report)

if __name__=='__main__':unittest.main()
