from pathlib import Path
import hashlib
import importlib.util
import json
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]


def read(p):
    return (ROOT/p).read_text(encoding='utf-8-sig')


class Release125(unittest.TestCase):
    def test_release_helper_has_no_video_stack(self):
        source = read('native/Recorder/AudioMain.cpp')
        self.assertIn('args[1]!=L"audio"', source)
        self.assertIn('GetDefaultAudioEndpoint(eRender,eMultimedia', source)
        self.assertIn('WaitForSingleObject(parent,0)', source)
        for forbidden in ('RecordingManager', 'MFStartup', 'D3D', 'CreateForWindow', 'screen:', 'videoSelfTest'):
            self.assertNotIn(forbidden, source)
        self.assertIn('MiniAudioRecorder::selfTest(output)', source)
        self.assertIn('audio.finish()', source)

    def test_archived_video_not_built_or_packaged_by_default(self):
        cmake = read('CMakeLists.txt')
        self.assertIn('option(FENGYIN_ARCHIVED_VIDEO_RECORDING "Build archived video-recording diagnostics, never for release" OFF)', cmake)
        recorder = read('native/Recorder/CMakeLists.txt')
        production = recorder.split('if(NOT FENGYIN_ARCHIVED_VIDEO_RECORDING)', 1)[1].split('return()', 1)[0]
        self.assertIn('AudioMain.cpp MiniAudioRecorder.cpp', production)
        self.assertNotIn('target_link_libraries(FengYinRecorder PRIVATE FengYinScreenRecorderLib)', production)
        build = read('scripts/build-windows.ps1')
        self.assertIn('-DFENGYIN_ARCHIVED_VIDEO_RECORDING=OFF', build)
        self.assertNotIn('prepare_recording.py', build)
        self.assertNotIn('Copy-Item $ScreenRecorderExe', build)
        self.assertNotIn('recording-backends\\*', build)
        self.assertIn('if ($RetiredCapture)', read('scripts/check-release.ps1'))

    def test_old_video_bridge_removed_new_gpu_helper_separate(self):
        self.assertNotIn('toggleWindowRecording', read('native/Source/MainComponent.cpp'))
        self.assertNotIn('toggleWindowRecording', read('prototype/app.js'))
        service = read('native/Source/WindowRecordingService.cpp')
        self.assertIn('bool WindowRecordingService::startAudio(bool prepareOnly)', service)
        self.assertNotIn('screen:', service)
        self.assertNotIn('FENGYIN_RECORDING_BACKEND', service)
        self.assertIn('RecordingPaths::video()', service)
        self.assertIn('FengYinScreenCapture.exe',service)
        self.assertNotIn('FengYinScreenRecorder.exe',service)

    def test_old_misleading_guide_audio_not_played(self):
        voice = read('prototype/voice-guide.js')
        self.assertIn("if(id==='GUIDE_RECORD')id='G010'", voice)
        self.assertNotIn('REC_VIDEO', voice)
        self.assertNotIn('录视频', voice)

    def test_no_native_vlc_dom_fullscreen(self):
        source = read('prototype/app.js')
        block = source.split('async function toggleFullscreen()', 1)[1].split('fullscreenButton.addEventListener', 1)[0]
        native = block.split('if(nativeVideoEngine)', 1)[1].split('try {', 1)[0]
        self.assertIn('setNativeVideoExpanded(', native)
        self.assertNotIn('requestFullscreen', native)
        self.assertIn('return;', native)

    def test_audio_source_preparation_verified_and_isolated(self):
        spec = importlib.util.spec_from_file_location('audio_dep125', ROOT/'scripts/prepare_audio_recording.py')
        module = importlib.util.module_from_spec(spec); spec.loader.exec_module(module)
        data = b'mock source\nALTERNATIVE 1 - Public Domain\nlicense\nALTERNATIVE 2 - MIT No Attribution\nlicense'
        with tempfile.TemporaryDirectory() as tmp:
            base=Path(tmp); header=base/'input.h'; header.write_bytes(data)
            with patch.object(module, 'SHA', hashlib.sha256(data).hexdigest()):
                destination=base/'audio'
                module.prepare(destination, header)
                manifest=json.loads((destination/'versions.json').read_text(encoding='utf-8'))
                self.assertTrue(manifest['audioOnly'])
                self.assertNotIn('screenRecorderLibCommit',manifest)
                self.assertFalse((destination/'srl').exists())
                self.assertEqual((destination/'miniaudio/miniaudio.h').read_bytes(),data)
                (destination/'licenses/ScreenRecorderLib-old.txt').write_text('old')
                with self.assertRaisesRegex(ValueError, 'clean audio-only'):
                    module.prepare(destination, header)
            with self.assertRaisesRegex(ValueError, 'SHA256'):
                module.prepare(base/'bad', header)

    def test_upgrade_does_not_delete_customer_recordings(self):
        installer=read('installer/FengYin.iss')
        section=installer.split('[InstallDelete]',1)[1].split('[Run]',1)[0]
        self.assertIn('FengYinScreenRecorder.exe', section)
        self.assertNotIn('{userdocs}', section)
        self.assertNotIn('*.mp4', section)
        self.assertNotIn('filesandordirs', section)


if __name__ == '__main__':
    unittest.main()
