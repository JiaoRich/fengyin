from pathlib import Path
import unittest
ROOT=Path(__file__).resolve().parents[1]

class Recording120(unittest.TestCase):
    def test_layered_child_windows_have_embedded_compatibility_contract(self):
        manifest=(ROOT/'native/Source/WindowsCompatibility.manifest').read_text(encoding='utf-8')
        self.assertIn('8e0f7a12-bfb3-4fe8-b9a5-48fd50a15a9a',manifest)
        cmake=(ROOT/'CMakeLists.txt').read_text(encoding='utf-8')
        for target in ['FengYin','FengYinCompactGlowTests']:
            self.assertIn(f'target_sources({target} PRIVATE native/Source/WindowsCompatibility.manifest)',cmake)

    def test_diagnostic_entry_and_bridge_removed_not_package_export(self):
        main=(ROOT/'native/Source/MainComponent.cpp').read_text(encoding='utf-8')
        js=(ROOT/'prototype/app.js').read_text(encoding='utf-8')
        html=(ROOT/'prototype/index.html').read_text(encoding='utf-8')
        self.assertNotIn('exportCurrentToneDiagnostic',main+js)
        self.assertNotIn('export-current-tone',html+js)
        self.assertIn('exportStudioDraft',main)
        self.assertIn('importStudioPackage',main)

    def test_system_audio_has_no_window_capture_dependency(self):
        source=(ROOT/'native/ScreenRecorder/main.cpp').read_text(encoding='utf-8')
        audio=source.split('if(audioOnly) {',1)[1].split('check(MFStartup',1)[0]
        self.assertIn('writer.openWave(output)',audio)
        self.assertIn('sound.start(ticks())',audio)
        self.assertNotIn('WindowFrames',audio)
        worker=source.split('worker=std::thread',1)[1].split('future.get()',1)[0]
        self.assertIn('append(std::move(packets),0)',worker)
        self.assertNotIn('ticks()-origin-2000000',worker)
        main=(ROOT/'native/Source/MainComponent.cpp').read_text(encoding='utf-8')
        self.assertIn('if(!visible && !windowRecorder.isAudioOnly())',main)
        self.assertIn('windowRecorder.start(nullptr,juce::Colours::black,false,true)',main)

    def test_native_title_uses_implicit_pointer_capture_not_css_coordinates(self):
        source=(ROOT/'native/Source/NativeCompactDragSurface.h').read_text(encoding='utf-8')
        for item in ['WM_POINTERDOWN','WM_POINTERUPDATE','WM_POINTERUP','WM_POINTERCAPTURECHANGED','WM_CANCELMODE','info.ptPixelLocation','GetCursorPos','ReleaseCapture']:
            self.assertIn(item,source)
        self.assertNotIn('screenX',source)
        self.assertIn('!EqualRect(&region,&next)',source)
        js=(ROOT/'prototype/app.js').read_text(encoding='utf-8')
        self.assertIn('reportNativeDragRegion',js)
        self.assertIn("bar.querySelectorAll('button')",js)

if __name__=='__main__':unittest.main()
