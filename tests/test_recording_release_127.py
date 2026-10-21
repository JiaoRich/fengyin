from pathlib import Path
import unittest

ROOT=Path(__file__).resolve().parents[1]


def read(name):
    return (ROOT/name).read_text(encoding='utf-8-sig')


class RecordingRelease127(unittest.TestCase):
    def test_installer_includes_new_gpu_helper_not_archived_stack(self):
        self.assertIn('"FengYinScreenCapture.exe"',read('scripts/check-release.ps1'))
        build=read('scripts/build-windows.ps1')
        self.assertIn('Copy-Item $GpuCaptureExe',build)
        self.assertIn('test_screen_capture_mux.py',build)
        self.assertIn('report_screen_capture_capabilities.py',build)
        self.assertNotIn('prepare_recording.py',build)
        self.assertIn('FENGYIN_ARCHIVED_VIDEO_RECORDING=OFF',build)

    def test_capture_stop_and_mux_are_distinct_explicit_actions(self):
        service=read('native/Source/WindowRecordingService.cpp')
        stop=service.split('void WindowRecordingService::stop()',1)[1].split('void WindowRecordingService::poll()',1)[0]
        self.assertNotIn('--mux-session',stop)
        self.assertIn('--mux-session',service.split('bool WindowRecordingService::generateVideo()',1)[1])
        self.assertIn('status.startsWith("captured;")',service)
        self.assertIn('captured=true',service)
        self.assertIn('bool WindowRecordingService::resumeScreenSession',service)

    def test_no_full_config_updates_added_to_meter_path(self):
        main=read('native/Source/MainComponent.cpp')
        self.assertIn('screenRecordingPending',main)
        self.assertIn('updateRecordingPerformanceGate()',main)
        self.assertIn('juce::Component::SafePointer<MainComponent>',main)
        gpu=read('native/ScreenCapture/GpuVideo.h')
        self.assertNotIn('D3D11_USAGE_STAGING',gpu)
        self.assertIn('if(!proven)',gpu)

    def test_audio_and_video_state_controls_do_not_fake_completion(self):
        js=read('prototype/app.js')
        self.assertIn("?'toggleScreenRecording':'toggleRecording'",js)
        self.assertIn('screenRecordingBusy=!!state.screenRecordingBusy',js)
        self.assertIn("saving?'正在保存，请稍等'",js)
        self.assertNotIn("state.screenRecordingPending?'生成视频'",js)
        self.assertNotIn('screenRecordingBusy=false; // Video recording is withdrawn',js)
