from pathlib import Path
import unittest

ROOT=Path(__file__).resolve().parents[1]


class Recording118Tests(unittest.TestCase):
    def test_explicit_display_and_default_speaker_no_mic(self):
        code=(ROOT/'native/ScreenRecorder/main.cpp').read_text(encoding='utf-8')
        self.assertIn('CreateForWindow(hwnd',code)
        self.assertIn('CreateForMonitor',code)
        self.assertIn('screen:',code)
        self.assertNotIn('GetDC(',code)
        self.assertIn('GetDefaultAudioEndpoint(eRender,eMultimedia',code)
        self.assertIn('AUDCLNT_STREAMFLAGS_LOOPBACK',code)
        self.assertNotIn('GetDefaultAudioEndpoint(eCapture',code)
        self.assertIn('sameEndpoint()',code)
        self.assertIn('IsIconic(hwnd)',code)
        self.assertIn('pool.Recreate(',code)

    def test_recording_is_packaged_and_actual_encoded_media_checked(self):
        build=(ROOT/'scripts/build-windows.ps1').read_text(encoding='utf-8-sig')
        self.assertNotIn('Copy-Item $ScreenRecorderExe',build)
        self.assertIn("@('--audio-selftest',",build)
        self.assertIn('$NativeProcess.WaitForExit(60000)',build)
        self.assertIn('$global:LASTEXITCODE = $NativeProcess.ExitCode',build)
        self.assertIn('verify_system_recording.py',build)
        # Old capture implementation remains archived, not packaged.
        helper=(ROOT/'native/ScreenRecorder/main.cpp').read_text(encoding='utf-8')
        self.assertIn('writer->Finalize()',helper)
        self.assertIn('MFAudioFormat_AAC',helper)
        self.assertIn('MFVideoFormat_H264',helper)
        self.assertIn('1920.0',helper)
        self.assertIn('1080.0',helper)

    def test_ui_exposes_real_video_event_not_fake_save(self):
        html=(ROOT/'prototype/index.html').read_text(encoding='utf-8')
        js=(ROOT/'prototype/app.js').read_text(encoding='utf-8')
        self.assertNotIn('id="record-options"',html)
        self.assertNotIn('id="record-video-choice"',html)
        self.assertNotIn("nativeEvent('toggleWindowRecording'",js)
        self.assertIn("$$('[data-record-kind]')",js)
        self.assertIn("?'toggleScreenRecording':'toggleRecording'",js)
        main=(ROOT/'native/Source/MainComponent.cpp').read_text(encoding='utf-8')
        self.assertNotIn('.withEventListener("toggleWindowRecording"',main)

    def test_async_helper_does_not_touch_realtime_audio(self):
        service=(ROOT/'native/Source/WindowRecordingService.cpp').read_text(encoding='utf-8')
        self.assertIn('process.start(args,0)',service)
        self.assertIn('partialFile.moveFileTo(destination)',service)
        self.assertNotIn('partialFile.deleteFile',service)
        host=(ROOT/'native/Source/PluginHostEngine.cpp').read_text(encoding='utf-8')
        self.assertNotIn('WindowRecording',host)


if __name__=='__main__':unittest.main()
