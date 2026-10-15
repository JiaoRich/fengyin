from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]

class TempoCCRegression(unittest.TestCase):
    def test_automatic_vibrato_uses_original_cc_fade(self):
        source = (ROOT/'native/Source/PluginHostEngine.cpp').read_text(encoding='utf-8')
        body = source.split('void PluginHostEngine::automaticVibratoChanged',1)[1].split('void PluginHostEngine::routeTechnique',1)[0]
        self.assertIn('routeTechnique(PerformanceTechnique::vibrato,input*fade)', body)
        self.assertNotIn('setValueNotifyingHost',body)
        self.assertNotIn('automaticVibratoDirect',source)
        self.assertNotIn('vibratoInputMin',source)

    def test_automation_retains_previous_native_timer(self):
        main = (ROOT/'native/Source/MainComponent.cpp').read_text(encoding='utf-8')
        timer = main.split('void MainComponent::timerCallback()',1)[1].split('void MainComponent::',1)[0]
        self.assertIn('midi.tickTechniqueAutomation()',timer)
        header = (ROOT/'native/Source/MidiInputService.h').read_text(encoding='utf-8')
        self.assertNotIn('HighResolutionTimer',header)
        self.assertNotIn('hiResTimerCallback',header)

if __name__ == '__main__': unittest.main()
