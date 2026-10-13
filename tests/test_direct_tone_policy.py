from pathlib import Path
import json
import wave
import unittest

ROOT = Path(__file__).resolve().parents[1]


class DirectTonePolicy(unittest.TestCase):
    def test_activation_is_read_only_and_editor_close_does_not_capture_state(self):
        host = (ROOT / 'native/Source/PluginHostEngine.cpp').read_text(encoding='utf-8')
        body = host.split('bool PluginHostEngine::applyStandardSwamExpressionCurve()', 1)[1].split('\nvoid ', 1)[0]
        self.assertIn('SwamTechniqueRouting::prepare', body)
        self.assertIn('if(configured.supported && configured.changed)', body)
        self.assertIn('state=original', body)
        self.assertNotIn('if (result.changed)', body) # Native curves are never rewritten.
        close = host.split('void PluginHostEngine::closePluginEditor', 1)[1].split('\nbool ', 1)[0]
        self.assertNotIn('applyStandardSwamExpressionCurve', close)
        self.assertNotIn('getStateInformation(', close)
        self.assertNotIn('window->onClose', host)

    def test_both_save_paths_capture_complete_state(self):
        main = (ROOT / 'native/Source/MainComponent.cpp').read_text(encoding='utf-8')
        for method in ('commitCurrentPreset', 'commitCustomPreset'):
            body = main.split('void MainComponent::' + method, 1)[1].split('\nvoid MainComponent::', 1)[0]
            for capture in ('captureInstrumentState()', 'captureEffectChain()', 'getToneStyle()'):
                self.assertIn(capture, body)
            for field in ('compressionRatio', 'saturation', 'harshControl', 'outputGain', 'bass', 'air'):
                self.assertIn('preset.' + field + ' = settings.' + field, body)

    def test_save_does_not_reapply_stale_ui_settings(self):
        main = (ROOT / 'native/Source/MainComponent.cpp').read_text(encoding='utf-8')
        body = main.split('.withEventListener("saveCustomPreset"', 1)[1].split('.withEventListener', 1)[0]
        self.assertNotIn('masterOutput.setToneStyle', body)

    def test_complete_state_is_authoritative(self):
        main = (ROOT / 'native/Source/MainComponent.cpp').read_text(encoding='utf-8')
        body = main.split('void MainComponent::completeLoadedPreset', 1)[1].split('\nvoid MainComponent::', 1)[0]
        self.assertIn('if (! hasFullState && preset.instrumentModelName', body)
        self.assertIn('if (! hasFullState)', body)

    def test_native_curve_routes_are_not_direct_parameter_overrides(self):
        host = (ROOT / 'native/Source/PluginHostEngine.cpp').read_text(encoding='utf-8')
        body = host.split('void PluginHostEngine::flushTechniqueValues()', 1)[1].split('\nbool ', 1)[0]
        self.assertIn('nativeTechniqueRoutes[index].load() == 0', body)
        midi = (ROOT / 'native/Source/MidiInputService.cpp').read_text(encoding='utf-8')
        self.assertNotIn('intelligentTechniques.process(', midi)
        self.assertNotIn('mapping.strength *', midi)

    def test_no_stale_volume_or_transpose_controls(self):
        html = (ROOT / 'prototype/index.html').read_text(encoding='utf-8')
        self.assertNotIn('id="limiter-ceiling"', html)
        self.assertIn('id="transpose-dialog"', html)
        self.assertIn('吹奏音量', html)
        guide = (ROOT / 'prototype/voice-guide.js').read_text(encoding='utf-8')
        self.assertNotIn("['演奏调','G003'", guide)
        self.assertNotIn("['电吹管调性','V004'", guide)

    def test_native_routes_do_not_leak_between_instruments(self):
        host = (ROOT / 'native/Source/PluginHostEngine.cpp').read_text(encoding='utf-8')
        for method in ('void PluginHostEngine::unload()', 'bool PluginHostEngine::applyStandardSwamExpressionCurve()'):
            body = host.split(method, 1)[1].split('\n}', 1)[0]
            self.assertIn('swamExpressionChannel.store(1', body)
            self.assertIn('nativeTechniqueRoutes) route.store(0', body)

    def test_offline_transpose_voice_assets(self):
        catalog = json.loads((ROOT / 'prototype/voice-catalog.js').read_text(encoding='utf-8').split('=', 1)[1].strip().rstrip(';'))
        clips = {item['id']: item for item in catalog['items']}
        ids = ['T001', 'T000'] + [f'{prefix}{n:02d}' for prefix in ('TU', 'TD') for n in range(1, 13)]
        for ident in ids:
            self.assertIn(ident, clips)
            with wave.open(str(ROOT / 'assets/voice' / (ident + '.wav')), 'rb') as clip:
                self.assertEqual(clip.getframerate(), 24000)
                self.assertEqual(clip.getnchannels(), 1)
                self.assertGreater(clip.getnframes(), 2400)
                self.assertNotEqual(set(clip.readframes(clip.getnframes())), {0})


if __name__ == '__main__':
    unittest.main()
