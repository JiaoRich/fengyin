from pathlib import Path
import hashlib,json,unittest,wave
ROOT=Path(__file__).resolve().parents[1]
class Performance121(unittest.TestCase):
 def test_audio_clock_is_never_repositioned_by_video_correction(self):
  s=(ROOT/'native/Source/MainComponent.cpp').read_text(encoding='utf-8')
  handler=s.split('.withEventListener("syncVideoPlayback"',1)[1].split('.withEventListener("seekVideo"',1)[0]
  self.assertNotIn('setPosition(',handler);self.assertIn('videoPlaybackClock',handler)
  playing=s.split('.withEventListener("setVideoPlaybackState"',1)[1].split('.withEventListener("syncVideoPlayback"',1)[0]
  self.assertNotIn('setPosition(',playing)
 def test_new_voice_assets_are_exact_and_same_voice(self):
  rows=json.loads((ROOT/'assets/voice/update-121-audit.json').read_text(encoding='utf-8'))
  self.assertEqual(len(rows),8)
  for row in rows:
   self.assertEqual(row['voice'],'x6_wuyediantai_mini');self.assertEqual(row['status'],'success')
   file=ROOT/'assets/voice'/(row['id']+'.wav');self.assertEqual(hashlib.sha256(file.read_bytes()).hexdigest(),row['sha256'])
   with wave.open(str(file)) as w:self.assertEqual(w.getframerate(),24000);self.assertGreater(w.getnframes(),1000)
 def test_timing_is_opt_in_no_audio_thread_disk_logging(self):
  header=(ROOT/'native/Source/PluginHostEngine.h').read_text(encoding='utf-8');self.assertIn('FENGYIN_AUDIO_TIMING',header)
  s=(ROOT/'native/Source/PluginHostEngine.cpp').read_text(encoding='utf-8').split('void RecordingAudioProcessorPlayer::audioDeviceIOCallbackWithContext',1)[1].split('juce::String PluginHostEngine::takeStageTimingReport',1)[0]
  self.assertNotIn('appendText',s);self.assertNotIn('writeToLog',s);self.assertIn('if(timingEnabled)',s)
 def test_proxy_work_is_idle_only_and_never_changes_audio(self):
  native=(ROOT/'native/Source/MainComponent.cpp').read_text(encoding='utf-8')
  block=native.split('void MainComponent::updateIdleVideoPreparation()',1)[1].split('std::optional<juce::WebBrowserComponent::Resource>',1)[0]
  for guard in ['videoPlaybackActive','recorder.isRecording()','windowRecorder.busy()','snapshot.breath','pluginLoading','effectLoading']:
   self.assertIn(guard,block)
  self.assertNotIn('accompaniment.setPosition',block)
  cache=(ROOT/'native/Source/VideoPlaybackCache.cpp').read_text(encoding='utf-8')
  for guard in ['singleBudget','cacheBudget','120000','shouldExit()','getBytesFreeOnVolume','InterProcessLock']:
   self.assertIn(guard,cache)
  self.assertIn('removeAllJobs(true,0)',cache)
  self.assertIn('"-an"',cache);self.assertIn('min(source_fps,24)',cache)
  self.assertIn('info(probe,partial)',cache)
 def test_windows_runs_real_native_proxy_and_decode_checks(self):
  build=(ROOT/'scripts/build-windows.ps1').read_text(encoding='utf-8-sig')
  self.assertIn('FengYinRecordingTests.exe',build);self.assertIn('verify_video_proxy.py',build)
  self.assertTrue((ROOT/'scripts/build-windows.ps1').read_bytes().startswith(b'\xef\xbb\xbf'))
if __name__=='__main__':unittest.main()
