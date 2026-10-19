from pathlib import Path
import unittest

ROOT=Path(__file__).resolve().parents[1]


class VlcWebViewDiagnostic(unittest.TestCase):
    def test_real_juce_webview_sdk_and_production_surface(self):
        cmake=(ROOT/'tools/vlc-webview-check/CMakeLists.txt').read_text()
        self.assertIn('juce_add_console_app(FengYinVlcWebViewCheck NEEDS_WEBVIEW2 TRUE)',cmake)
        self.assertIn('native/Source/VlcVideoSurface.cpp',cmake)
        self.assertIn('native/Source/VlcPlaybackEngine.cpp',cmake)
        self.assertIn('FENGYIN_VLC_TEST_PCM=1',cmake)

    def test_actual_input_and_distinct_failure_detection(self):
        source=(ROOT/'tools/vlc-webview-check/main.cpp').read_text()
        for required in ('SendInput(2,inputs,sizeof(INPUT))','GetGUIThreadInfo(uiThread,&info)',
                         'MiniDumpWriteDump','native heartbeat stalled','web heartbeat stalled',
                         'if(++cycle==20)','testPcmNonzero.load()==0'):
            self.assertIn(required,source)
        runner=(ROOT/'scripts/check_vlc_webview.py').read_text()
        self.assertIn("('immediate','deferred','attached-immediate','attached-deferred')",runner)
        self.assertIn('acknowledgements==20',runner)
        self.assertIn("all(r['passed'] for r in results)",runner)
