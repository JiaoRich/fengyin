from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]


class PostEnumerationRestart(unittest.TestCase):
    def test_production_restart_precedes_open_even_with_zero_changes(self):
        code=(ROOT/'audio-engine/windows/Asio4AllOutput.cpp').read_text(encoding='utf-8')
        start=code.index('if (configuration.enumerated)')
        reset=code.index('requestFengYinAsioReinitialisation()',start)
        opening=code.index('device->open(',reset)
        self.assertLess(start,reset)
        self.assertLess(reset,opening)
        self.assertNotIn('configuration.configured',code[start:reset])
        api=(ROOT/'audio-engine/windows/RealtekAsioConfiguration.h').read_text(encoding='utf-8')
        self.assertLess(api.index('api->enumerate();'),api.index('context.result.enumerated = true'))

    def test_halo_uses_software_composition_and_dwm_not_only_binary_region(self):
        halo=(ROOT/'native/Source/CompactWindowGlow.h').read_text(encoding='utf-8')
        window=(ROOT/'native/Source/Main.cpp').read_text(encoding='utf-8')
        self.assertIn('setCurrentRenderingEngine(0)',halo)
        self.assertIn('HWND_TOP',halo)
        self.assertLess(window.index('DwmSetWindowAttribute'),window.index('CreateRoundRectRgn'))
