from pathlib import Path
import unittest

ROOT=Path(__file__).resolve().parents[1]


class Recording118Tests(unittest.TestCase):
    def test_only_window_and_default_speaker_no_desktop_or_mic(self):
        code=(ROOT/'native/ScreenRecorder/main.cpp').read_text(encoding='utf-8')
        self.assertIn('CreateForWindow(hwnd',code)
        self.assertNotIn('CreateForMonitor',code)
        self.assertNotIn('GetDC(',code)
        self.assertIn('GetDefaultAudioEndpoint(eRender,eMultimedia',code)
        self.assertIn('AUDCLNT_STREAMFLAGS_LOOPBACK',code)
        self.assertNotIn('GetDefaultAudioEndpoint(eCapture',code)
        self.assertIn('sameEndpoint()',code)
        self.assertIn('IsIconic(hwnd)',code)
        self.assertIn('pool.Recreate(',code)

    def test_recording_is_packaged_and_actual_encoded_media_checked(self):
        build=(ROOT/'scripts/build-windows.ps1').read_text(encoding='utf-8-sig')
        self.assertIn('Copy-Item $ScreenRecorderExe',build)
        self.assertIn("@('--selftest',",build)
        self.assertIn('$RecorderProcess.WaitForExit(60000)',build)
        self.assertIn('$global:LASTEXITCODE = $RecorderProcess.ExitCode',build)
        self.assertNotIn('& $ScreenRecorderExe --selftest',build)
        self.assertIn('verify_screen_recording.py',build)
        helper=(ROOT/'native/ScreenRecorder/main.cpp').read_text(encoding='utf-8')
        self.assertIn('writer->Finalize()',helper)
        self.assertIn('MFAudioFormat_AAC',helper)
        self.assertIn('MFVideoFormat_H264',helper)
        self.assertIn('1280.0',helper)
        self.assertIn('720.0',helper)

    def test_ui_exposes_real_video_event_not_fake_save(self):
        html=(ROOT/'prototype/index.html').read_text(encoding='utf-8')
        js=(ROOT/'prototype/app.js').read_text(encoding='utf-8')
        self.assertIn('id="record-options"',html)
        self.assertIn('id="record-video-choice"',html)
        video=js.split("$('#record-video-choice').onclick",1)[1].split("$('#screen-recordings')",1)[0]
        self.assertIn("nativeEvent('toggleWindowRecording'",video)
        self.assertIn('Windows版风吟',video)
        self.assertIn('screenRecordingStopping',js)
        self.assertIn('screenRecordingBusy',(ROOT/'prototype/voice-guide.js').read_text(encoding='utf-8'))

    def test_async_helper_does_not_touch_realtime_audio(self):
        service=(ROOT/'native/Source/WindowRecordingService.cpp').read_text(encoding='utf-8')
        self.assertIn('process.start(args,0)',service)
        self.assertIn('partialFile.moveFileTo(destination)',service)
        self.assertNotIn('partialFile.deleteFile',service)
        host=(ROOT/'native/Source/PluginHostEngine.cpp').read_text(encoding='utf-8')
        self.assertNotIn('WindowRecording',host)


if __name__=='__main__':unittest.main()
