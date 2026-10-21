from pathlib import Path
import unittest
import wave
import json
import hashlib

ROOT=Path(__file__).resolve().parents[1]
def read(path):return (ROOT/path).read_text(encoding='utf-8-sig')

class RecordingInteraction128(unittest.TestCase):
    def test_preparation_and_cue_are_native_nonblocking_handshake(self):
        main=read('native/Source/MainComponent.cpp')
        for token in ['startAudio(true)','startScreen(handle,true)','windowRecorder.prepared()',
                      'voicePrompts.play(data,size,700000002','windowRecorder.beginCapture()']:
            self.assertIn(token,main)
        screen=main.split('void MainComponent::toggleScreenRecording()',1)[1].split('void MainComponent::toggleRecording()',1)[0]
        self.assertNotIn('showOkCancelBox',screen)
        js=read('prototype/app.js')
        self.assertNotIn('await window.fengyinVoice.beforeRecording',js)
        self.assertNotIn('id="record-kind"',read('prototype/index.html'))
        self.assertIn('data-record-kind="audio"',read('prototype/index.html'))
        self.assertIn('data-record-kind="video"',read('prototype/index.html'))

    def test_begin_gate_excludes_preparation_samples_and_frames(self):
        audio=read('native/Recorder/AudioMain.cpp')
        self.assertLess(audio.index('audio.setRecordingEnabled(!armed)'),audio.index('audio.start(output,id)'))
        self.assertLess(audio.index('std::filesystem::exists(args[8])'),audio.index('audio.setRecordingEnabled(true)'))
        mini=read('native/Recorder/MiniAudioRecorder.cpp')
        push=mini.split('void push(',1)[1].split('void drain()',1)[0]
        self.assertIn('recordingEnabled.load',push)
        self.assertNotIn('sleep',push)
        screen=read('native/ScreenCapture/main.cpp')
        self.assertIn('std::max(heldStamp,beginQpc)',screen)
        self.assertIn('if(held&&!announced){status(folder,"ready")',screen)
        self.assertIn('origin-audio->firstQpc()',screen)

    def test_automatic_save_temporarily_gates_performance_without_dsp_changes(self):
        main=read('native/Source/MainComponent.cpp')
        timer=main.split('void MainComponent::timerCallback()',1)[1].split('\nvoid ',1)[0]
        self.assertIn('if(windowRecorder.needsVideoGeneration())windowRecorder.generateVideo()',timer)
        gate=main.split('void MainComponent::updateRecordingPerformanceGate()',1)[1].split('\nvoid ',1)[0]
        for token in ['midi.setPerformanceSink(nullptr)','pluginHost.resetPerformance()','midi.setPerformanceSink(&pluginHost)']:
            self.assertIn(token,gate)
        for token in ['applyOutputSetup','setGain','setMix','pauseAccompaniment','selectDeviceType']:
            self.assertNotIn(token,gate)

    def test_cleanup_only_successful_owned_known_files_not_recursive_sweep(self):
        service=read('native/Source/WindowRecordingService.cpp')
        cleanup=service.split('void WindowRecordingService::cleanCompletedSession()',1)[1].split('WindowRecordingService::~',1)[0]
        for token in ['getParentDirectory()!=temporaryFolder()','isSymbolicLink()','fengyin-session-v2','owned.contains(name)']:
            self.assertIn(token,cleanup)
        self.assertNotIn('deleteRecursively',cleanup)
        self.assertIn('partialFile.moveFileTo(destination)){lastError.clear();cleanCompletedSession();}',service)
        for folder in ['录音文件','录屏文件','临时文件']:
            self.assertIn(folder,read('native/Source/RecordingPaths.h'))

    def test_advanced_settings_and_compact_layout_contract(self):
        html=read('prototype/index.html');js=read('prototype/app.js');css=read('prototype/styles.css')
        self.assertIn('data-page="settings"><span>⚙</span>高级设置',html)
        self.assertNotIn('data-page="audio"',html)
        for removed in ['id="low-performance"','id="audio-auto-optimize"','启动时进入演奏页面','自动保存设置','检查软件更新']:
            self.assertNotIn(removed,html)
        self.assertNotIn('.withEventListener("optimiseAudioSettings"',read('native/Source/MainComponent.cpp'))
        self.assertIn("$('#license-code').hidden=permanent",js)
        self.assertIn('id="advanced-voice"',html);self.assertIn('id="advanced-studio"',html)
        self.assertIn('body.compact-mode .compact-action-row{display:grid!important;grid-template-columns:repeat(2,minmax(0,1fr))',html)
        self.assertIn('body:not(.compact-mode) .compact-action-row #music-tempo',css)

    def test_new_cues_are_offline_pcm_not_runtime_network_speech(self):
        for filename in ['REC_PREP_AUDIO.wav','REC_PREP_SCREEN.wav','REC_BEGIN_BEEP.wav','REC_SAVED.wav']:
            with wave.open(str(ROOT/'assets/voice'/filename),'rb') as stream:
                self.assertEqual((stream.getnchannels(),stream.getsampwidth(),stream.getframerate()),(1,2,24000))
                self.assertGreater(stream.getnframes(),2000)

    def test_recording_speech_is_verified_original_xfyun_voice(self):
        audit=json.loads(read('assets/voice/recording-xfyun-audit.json'))
        self.assertEqual({x['id'] for x in audit},{'REC_PREP_AUDIO','REC_PREP_SCREEN','REC_SAVED'})
        for item in audit:
            self.assertEqual(item['voice'],'x6_wuyediantai_mini')
            self.assertEqual(hashlib.sha256((ROOT/'assets/voice'/f"{item['id']}.wav").read_bytes()).hexdigest(),item['sha256'])
        main=read('native/Source/MainComponent.cpp')
        timer=main.split('void MainComponent::timerCallback()',1)[1].split('\nvoid ',1)[0]
        self.assertNotIn('"recordingSaved"',timer)  # No duplicate Windows completion event.

if __name__=='__main__':unittest.main()
