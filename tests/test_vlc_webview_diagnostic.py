from pathlib import Path
import unittest

ROOT=Path(__file__).resolve().parents[1]


class VlcWebViewDiagnostic(unittest.TestCase):
    def test_real_juce_webview_sdk_and_production_surface(self):
        cmake=(ROOT/'tools/vlc-webview-check/CMakeLists.txt').read_text(encoding='utf-8')
        self.assertIn('juce_add_console_app(FengYinVlcWebViewCheck NEEDS_WEBVIEW2 TRUE)',cmake)
        self.assertIn('native/Source/VlcVideoSurface.cpp',cmake)
        self.assertIn('native/Source/VlcPlaybackEngine.cpp',cmake)
        self.assertIn('FENGYIN_VLC_TEST_PCM=1',cmake)

    def test_actual_input_and_distinct_failure_detection(self):
        source=(ROOT/'tools/vlc-webview-check/main.cpp').read_text(encoding='utf-8')
        for required in ('SendInput(2,inputs,sizeof(INPUT))','GetGUIThreadInfo(uiThread,&info)',
                         'MiniDumpWriteDump','native heartbeat stalled','web heartbeat stalled',
                         'if(++cycle==20)','testPcmNonzero.load()==0'):
            self.assertIn(required,source)
        runner=(ROOT/'scripts/check_vlc_webview.py').read_text(encoding='utf-8')
        self.assertIn("('immediate','deferred','attached-immediate','attached-deferred')",runner)
        self.assertIn('acknowledgements==20',runner)
        self.assertIn("all(r['passed'] for r in results)",runner)

    def test_production_stable_hwnd_and_separate_native_visibility(self):
        source=(ROOT/'native/Source/VlcVideoSurface.cpp').read_text(encoding='utf-8')
        header=(ROOT/'native/Source/VlcVideoSurface.h').read_text(encoding='utf-8')
        self.assertIn('class VlcVideoSurface final : public juce::Component',header)
        self.assertNotIn('public juce::HWNDComponent',header)
        self.assertLess(source.index('nativeWindow=hwnd'),source.index('engine=std::make_unique<VlcPlaybackEngine>'))
        self.assertNotIn('SetParent(',source)
        viewport=source.split('void VlcVideoSurface::updateViewport',1)[1]
        self.assertNotIn('setVisible(',viewport)
        self.assertIn('setVlcChildVisible(hwnd,parent,show && isShowing())',viewport)
        self.assertIn('if(!bounds.isEmpty()',viewport)
        main=(ROOT/'native/Source/MainComponent.cpp').read_text(encoding='utf-8')
        self.assertIn('addAndMakeVisible(vlcVideo)',main)

    def test_combined_input_is_required_release_gate(self):
        cmake=(ROOT/'CMakeLists.txt').read_text(encoding='utf-8')
        self.assertIn('add_test(NAME vlc_webview_interaction',cmake)
        build=(ROOT/'scripts/build-windows.ps1').read_text(encoding='utf-8-sig')
        self.assertIn('--target FengYinVlcWebViewCheck',build)
        self.assertIn("-R '^vlc_webview_interaction$'",build)
        workflow=(ROOT/'.github/workflows/windows-build.yml').read_text(encoding='utf-8')
        self.assertIn('path: build-windows/evidence/',workflow)
        source=(ROOT/'tools/vlc-webview-check/main.cpp').read_text(encoding='utf-8')
        self.assertIn('firstPictureVisible()',source)
        self.assertIn('FAIL preview emitted accompaniment',source)
        self.assertIn('surface.getState().generation==generation',source)
