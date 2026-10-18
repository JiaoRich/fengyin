from pathlib import Path
import importlib.util
import tempfile
import unittest
from unittest import mock
import subprocess
import json

ROOT=Path(__file__).resolve().parents[1]
def read(path):return (ROOT/path).read_text(encoding='utf-8-sig')

class Recording123(unittest.TestCase):
    def test_explicit_backend_and_scope_no_silent_legacy_fallback(self):
        service=read('native/Source/WindowRecordingService.cpp')
        self.assertIn('FENGYIN_RECORDING_BACKEND',service)
        self.assertIn('legacy?"FengYinScreenRecorder.exe":"FengYinRecorder.exe"',service)
        self.assertEqual(service.count('process.start('),1)
        self.assertIn('"screen:"+',service)
        for filename in ['Use-new-recording.cmd','Use-legacy-recording.cmd']:
            cmd=read('scripts/recording-backends/'+filename)
            self.assertIn('tasklist',cmd)
            self.assertIn('setlocal',cmd)
        main=read('native/Source/MainComponent.cpp')
        mode=main.split('void MainComponent::requestCompactMode',1)[1].split('\nvoid ',1)[0]
        self.assertNotIn('windowRecorder.stop()',mode)

    def test_third_party_capture_is_display_loopback_only(self):
        source=read('native/Recorder/main.cpp')
        self.assertIn('RecordingSourceType::Display',source)
        self.assertIn('MonitorFromWindow(hwnd',source)
        self.assertIn('GetDefaultAudioEndpoint(eRender,eMultimedia',source)
        self.assertIn('AudioClientKind::EndpointLoopback',source)
        self.assertNotIn('RecordingSourceType::Window',source)
        self.assertNotIn('EndpointCapture',source)
        self.assertIn('WaitForSingleObject(parent,0)',source)
        self.assertIn('!EqualRect(&current.rcMonitor,&info.rcMonitor)',source)
        self.assertIn('recorder.EndRecording()',source)
        self.assertIn('IsVideoFramePreviewEnabled=false',source)

    def test_miniaudio_callback_is_bounded_no_disk_or_alloc(self):
        source=read('native/Recorder/MiniAudioRecorder.cpp')
        push=source.split('void push(',1)[1].split('void drain()',1)[0]
        self.assertIn('ma_pcm_rb_acquire_write',push)
        self.assertIn('failed.store(true)',push)
        for word in ['new ', 'resize(', 'ma_encoder_write','std::lock_guard']:
            self.assertNotIn(word,push)
        self.assertIn('ma_device_type_loopback',source)
        self.assertIn('ma_share_mode_shared',source)
        self.assertIn('ma_encoder_config_init(ma_encoding_format_wav,ma_format_s16,2,48000)',source)
        self.assertIn('noAutoStreamRouting=MA_TRUE',source)
        self.assertIn('std::memset(out,0,count*4)',source)

    def test_recorder_d3d_header_order_190_regression(self):
        source=read('native/Recorder/main.cpp')
        self.assertIn('#include <d3d10_1.h>',source)
        self.assertNotIn('#include <d3d10.h>',source)
        self.assertLess(source.index('#include <d3d10_1.h>'),source.index('#include "RecordingManager.h"'))
        self.assertIn('ComPtr<ID3D10Multithread>',source)

    def test_media_diagnostic_collects_remaining_failures_and_cannot_hide_missing_output(self):
        spec=importlib.util.spec_from_file_location('media_check',ROOT/'scripts/check_media_integration.py')
        module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
        with tempfile.TemporaryDirectory() as temp:
            calls=[]
            def execute(command,**kwargs):
                calls.append(command)
                return subprocess.CompletedProcess(command,1 if len(calls)==1 else 0,b'fixture',b'')
            with mock.patch.object(module.subprocess,'run',side_effect=execute):
                self.assertEqual(module.check(temp,temp,temp),1)
            results=json.loads((Path(temp)/'evidence/results.json').read_text(encoding='utf-8'))
            self.assertEqual(len(results),17)
            self.assertEqual(results[0]['code'],1)
            self.assertEqual(results[-1]['name'],'vlc-lifecycle')
            self.assertIn('srl-decode-video',[r['name'] for r in results])
            self.assertIn('srl-explicit-rate-decode-audio',[r['name'] for r in results])
        # Experimental define is only in the small diagnostic target, never
        # in either production CMake file. It is not a silently applied fix.
        self.assertNotIn('FENGYIN_RECORDER_EXPLICIT_RATE_PROBE',read('CMakeLists.txt'))
        self.assertNotIn('FENGYIN_RECORDER_EXPLICIT_RATE_PROBE',read('native/Recorder/CMakeLists.txt'))

    def test_real_new_and_legacy_encoding_checked_before_main(self):
        build=read('scripts/build-windows.ps1')
        for marker in ['ScreenRecorderLib actual decoded video/audio','Miniaudio actual PCM continuity','System recording PCM continuity check']:
            self.assertLess(build.index(marker),build.index('Invoke-Checked "主程序编译"'))
        self.assertIn('$NativeProcess.WaitForExit(60000)',build)
        self.assertIn('$global:LASTEXITCODE = $NativeProcess.ExitCode',build)
        release=read('scripts/check-release.ps1')
        for name in ['FengYinRecorder.exe','FengYinScreenRecorder.exe','ScreenRecorderLib-MIT.txt','miniaudio-0.11.23.h']:
            self.assertIn(name,release)

    def test_dependency_verifier_rejects_unverified_bytes(self):
        spec=importlib.util.spec_from_file_location('record_deps',ROOT/'scripts/prepare_recording.py')
        module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
        with tempfile.TemporaryDirectory() as temp:
            path=Path(temp)/'bad';path.write_bytes(b'not a library')
            with self.assertRaisesRegex(ValueError,'SHA256 mismatch'):
                module.verified(module.MA_URL,path,module.MA_SHA)

    def test_srl_uses_upstream_project_not_orphaned_sources(self):
        spec=importlib.util.spec_from_file_location('record_deps',ROOT/'scripts/prepare_recording.py')
        module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
        with tempfile.TemporaryDirectory() as temp:
            root=Path(temp)
            project=root/'ScreenRecorderLibNative.vcxproj'
            project.write_text('<Project xmlns="http://schemas.microsoft.com/developer/msbuild/2003"><ItemGroup>'
                '<ClCompile Include="RecordingManager.cpp"/><ClCompile Include="Screengrab.cpp"/>'
                '</ItemGroup></Project>')
            for name in ['RecordingManager.cpp','screengrab.cpp','Resizer.cpp']:
                (root/name).write_text('// fixture')
            self.assertEqual(module.native_sources(root),['RecordingManager.cpp','screengrab.cpp'])
            (root/'RecordingManager.cpp').unlink()
            with self.assertRaisesRegex(ValueError,'Missing upstream compile file'):
                module.native_sources(root)
        cmake=read('native/Recorder/CMakeLists.txt')
        self.assertIn('include("${SRL_ROOT}/upstream-sources.cmake")',cmake)
        self.assertNotIn('file(GLOB SRL_SOURCES',cmake)

    def test_vlc_windows_path_188_regression(self):
        source=read('native/Source/VlcPlaybackEngine.cpp')
        self.assertIn("std::replace(nativePath.begin(),nativePath.end(),'/','\\\\');",source)
        self.assertIn('libvlc_media_new_path(instance,nativePath.c_str())',source)
        test=read('tests/test_vlc_playback.cpp')
        self.assertIn('风吟 VLC 回归.mp4',test)
        self.assertIn("std::replace(file.begin(),file.end(),'\\\\','/');",test)

if __name__=='__main__':unittest.main()
