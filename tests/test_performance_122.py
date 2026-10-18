from pathlib import Path
import unittest

ROOT=Path(__file__).resolve().parents[1]

class Performance122(unittest.TestCase):
    def test_cold_windows_probe_has_bounded_headroom_and_remains_cancellable(self):
        source=(ROOT/'native/Source/VideoPlaybackCache.cpp').read_text(encoding='utf-8')
        probe=source.split('VideoPlaybackCache::Info info(',1)[1].split('static bool valid',1)[0]
        self.assertIn('},60000)',probe)
        self.assertIn('process.isRunning()&&!shouldExit()',source)
        self.assertIn('process.kill()',source)

    def test_no_video_decoder_or_source_read_lock_in_callback(self):
        source=(ROOT/'native/Source/AccompanimentAudioService.cpp').read_text(encoding='utf-8')
        selection=source.split('findDecodableFile',1)[1].split('bool AccompanimentAudioService::loadForVideo',1)[0]
        self.assertNotIn('createReaderFor(videoFile)',selection)
        self.assertIn('".mp3"',selection) # Preserve existing explicitly supplied audio sidecars.
        self.assertIn('public juce::BufferingAudioReader',source)
        self.assertIn('setReadTimeout(0)',source)
        self.assertIn('transport.setSource(readerSource.get(),0,nullptr,sourceRate)',source)

    def test_playback_is_720_but_recording_remains_1080(self):
        source=(ROOT/'native/Source/VideoPlaybackCache.cpp').read_text(encoding='utf-8')
        self.assertIn('720p24-h264-v2',source)
        self.assertIn("min(iw,1280)",source)
        self.assertIn('inputInfo.duration*4000.0',source)
        self.assertIn('timeout after ',source)
        recorder=ROOT/'native/ScreenRecorder/main.cpp'
        self.assertIn('1920',recorder.read_text(encoding='utf-8'))

    def test_synthetic_notes_are_explicit_bounded_and_disconnected_only(self):
        header=(ROOT/'native/Source/MainComponent.h').read_text(encoding='utf-8')
        source=(ROOT/'native/Source/MainComponent.cpp').read_text(encoding='utf-8')
        self.assertIn('FENGYIN_PERFORMANCE_NOTES',header)
        self.assertIn('FENGYIN_AUDIO_TIMING',header)
        block=source.split('if(performanceNotesEnabled)',1)[1].split('windowRecorder.poll()',1)[0]
        self.assertIn('!midi.getSnapshot().deviceConnected',block)
        self.assertIn('now-performanceNotesStarted<180',block)
        self.assertIn('noteOff',block)
        self.assertNotIn('setParameter',block)

    def test_processor_changes_never_gate_accompaniment(self):
        header=(ROOT/'native/Source/PluginHostEngine.h').read_text(encoding='utf-8')
        source=(ROOT/'native/Source/PluginHostEngine.cpp').read_text(encoding='utf-8')
        self.assertIn('processorChangeGate.change',header)
        callback=source.split('void RecordingAudioProcessorPlayer::audioDeviceIOCallbackWithContext',1)[1].split('juce::String PluginHostEngine::takeStageTimingReport',1)[0]
        self.assertIn('processorChangeGate.processIfAvailable',callback)
        self.assertLess(callback.index('if(!processed)'),callback.index('accompaniment->mixInto'))

if __name__=='__main__':unittest.main()
