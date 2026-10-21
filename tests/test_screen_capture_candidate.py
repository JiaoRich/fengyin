from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]


class ScreenCaptureCandidate(unittest.TestCase):
    def test_gpu_helper_is_separate_process_not_linked_into_host(self):
        cmake = (ROOT / 'native/ScreenCapture/CMakeLists.txt').read_text(encoding='utf-8')
        production = (ROOT / 'CMakeLists.txt').read_text(encoding='utf-8')
        self.assertNotIn('add_subdirectory(native/ScreenCapture)', production)
        self.assertIn('add_executable(FengYinScreenCapture native/ScreenCapture/main.cpp)',production)
        self.assertNotIn('install(TARGETS', cmake)
        service = (ROOT / 'native/Source/WindowRecordingService.cpp').read_text(encoding='utf-8')
        self.assertNotIn('FengYinScreenCaptureProbe', service)

    def test_no_full_frame_readback_or_software_fallback(self):
        gpu = (ROOT / 'native/ScreenCapture/GpuVideo.h').read_text(encoding='utf-8')
        self.assertNotIn('D3D11_USAGE_STAGING', gpu)
        self.assertNotIn('context->Map(', gpu)
        self.assertIn('MFCreateDXGISurfaceBuffer', gpu)
        self.assertIn('GetContiguousLength(&length)', gpu)
        self.assertIn('buffer->SetCurrentLength(length)', gpu)
        self.assertIn('MFT_ENUM_HARDWARE_URL_Attribute', gpu)
        self.assertIn('MF_SA_D3D11_AWARE', gpu)
        self.assertIn('tracking->SetAllocator', gpu)
        self.assertIn('ComPtr<ID3D11Multithread>', gpu)
        self.assertIn('context.As(&multithread)', gpu)
        self.assertNotIn('ID3D10Multithread', gpu)
        self.assertLess(gpu.index('if(!proven)'), gpu.index('writer->BeginWriting()'))

    def test_pcm_producer_never_encodes_or_writes_to_disk(self):
        audio = (ROOT / 'native/ScreenCapture/TimedAudio.h').read_text(encoding='utf-8')
        callback = audio.split('producer=std::thread(', 1)[1]
        self.assertNotIn('writer.append', callback)
        self.assertNotIn('WriteSample', callback)
        self.assertIn('AUDCLNT_BUFFERFLAGS_DATA_DISCONTINUITY', callback)
        self.assertIn('produced.store(index+1,std::memory_order_release)', callback)
        self.assertIn('Audio disk queue overflow', callback)

    def test_mux_is_explicit_and_failure_cannot_remove_originals(self):
        main = (ROOT / 'native/ScreenCapture/main.cpp').read_text(encoding='utf-8')
        capture = main.split('static void record(', 1)[1].split('int wmain(', 1)[0]
        self.assertNotIn('mux(', capture)
        self.assertIn('--mux-session', main)
        mux = (ROOT / 'native/ScreenCapture/OfflineMux.h').read_text(encoding='utf-8')
        self.assertNotIn('remove(', mux)
        self.assertIn('MFVideoFormat_H264', mux)
        self.assertIn('refusing silent padding', mux)

    def test_diagnostic_dispatch_does_not_trigger_full_installer(self):
        workflow = (ROOT / '.github/workflows/windows-build.yml').read_text(encoding='utf-8')
        release = workflow.split('  windows-release:', 1)[1]
        self.assertIn('!inputs.screen_capture_probe', release.split('steps:', 1)[0])
        self.assertIn('targetPerformanceAccepted=False', (ROOT / 'scripts/test_screen_capture_mux.py').read_text(encoding='utf-8'))
