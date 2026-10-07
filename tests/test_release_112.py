from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]

class Release112(unittest.TestCase):
    def test_borderless_work_area(self):
        source=(ROOT/'native/Source/Main.cpp').read_text()
        for text in ['setUsingNativeTitleBar(false)', 'setTitleBarHeight(0)',
                     'area.reduced(12)', 'juce::BorderSize<int>(0)']:
            self.assertIn(text,source)

    def test_exact_endpoint_before_mutation(self):
        source=(ROOT/'audio-engine/windows/AsioEndpointSelection.h').read_text()
        self.assertLess(source.index('if (chosenD < 0'),source.index('auto change ='))
        self.assertIn('if (c.attempted) return FALSE',source)
        self.assertIn('api->callback(nullptr,nullptr)',source)
        self.assertIn('c.changed.rbegin()',source)

    def test_sound_check_cancel_and_failure(self):
        source=(ROOT/'prototype/app.js').read_text()
        self.assertIn("soundCheckDialog.addEventListener('cancel'",source)
        self.assertIn("disabled=!String(message).startsWith('正在播放')",source)
        self.assertIn('soundCheckIndex >= soundCheckChoices.length',source)

    def test_manual_output_not_overwritten(self):
        source=(ROOT/'audio-engine/windows/AudioEngineMain.cpp').read_text()
        self.assertIn('!manualPhysicalOutput',source)

if __name__=='__main__': unittest.main()
