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

    def test_baseline_reversed_order_and_strict_picture_gate(self):
        import importlib.util
        spec=importlib.util.spec_from_file_location('video_check',ROOT/'scripts/check_vlc_webview.py')
        runner=importlib.util.module_from_spec(spec);spec.loader.exec_module(runner)
        cases=list(runner.case_order(2))
        self.assertEqual(len(cases),10)
        self.assertEqual(cases[0],(1,'web-only'))
        self.assertEqual(cases[5],(2,'web-only'))
        self.assertEqual([m for _,m in cases[1:5]],list(reversed([m for _,m in cases[6:]])))
        clicks='PASS physical click cycle=\n'*20
        self.assertTrue(runner.check_result('web-only',0,False,clicks)['passed'])
        self.assertFalse(runner.check_result('immediate',0,False,clicks)['passed'])
        complete=clicks+'PASS real paused first picture generation=\n'*4
        self.assertTrue(runner.check_result('immediate',0,False,complete)['passed'])
        self.assertFalse(runner.check_result('immediate',1,False,complete)['passed'])
        self.assertFalse(runner.check_result('immediate',0,True,complete)['passed'])
        source=(ROOT/'tools/vlc-webview-check/main.cpp').read_text(encoding='utf-8')
        self.assertIn('runDispatchLoop();',source)
        self.assertNotIn('runDispatchLoopUntil(',source)
        self.assertIn('failed-window.bmp',source)
        self.assertIn('picture rgb=',source)

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

    def test_release_frontend_not_only_synthetic_buttons(self):
        source=(ROOT/'tools/vlc-webview-check/ProductionUiCheck.h').read_text(encoding='utf-8')
        for item in ('FENGYIN_DIAGNOSTIC_FRONTEND','juce::URL(juce::File(',
                     '"#change-video"','"#fullscreen"','"#play-button"','"#video-volume"',
                     'SendInput(2,input,sizeof(INPUT))','firstPictureVisible()',
                     'backendState','nativeVideoState','if(++cycle==20)',
                     'InitializeTouchInjection','InjectTouchInput','POINTER_FLAG_UP'):
            self.assertIn(item,source)
        self.assertNotIn('.click()',source)
        self.assertIn("(args.rounds+1,'production-ui')",(ROOT/'scripts/check_vlc_webview.py').read_text(encoding='utf-8'))
