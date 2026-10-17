from pathlib import Path
import hashlib
import json
import unittest
import zipfile
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]


class KensaBend118Tests(unittest.TestCase):
    def test_received_binding_and_all_non_identity_content_are_preserved(self):
        latest=ROOT/'assets/tone-packages/kensa-refresh-119-audit.json'
        audit = json.loads((latest if latest.exists() else ROOT/'assets/tone-packages/kensa-bend-118-audit.json').read_text(encoding='utf-8'))
        original = ROOT/audit['source']
        packaged = ROOT/'assets/tone-packages/kontakt-kensa.fytonepack'
        self.assertEqual(hashlib.sha256(original.read_bytes()).hexdigest(), audit['sourceSha256'])
        self.assertEqual(hashlib.sha256(packaged.read_bytes()).hexdigest(), audit['packageSha256'])
        with zipfile.ZipFile(original) as z:
            before = ET.fromstring(z.read('tone.xml')).find('PRESET')
        with zipfile.ZipFile(packaged) as z:
            self.assertIsNone(z.testzip())
            manifest = json.loads(z.read('manifest.json'))
            for entry in manifest['files']:
                self.assertEqual(hashlib.sha256(z.read(entry['path'])).hexdigest(), entry['sha256'])
            after = ET.fromstring(z.read('tone.xml')).find('PRESET')
        self.assertEqual(after.get('instrumentKey'), 'kontakt-kensa')
        self.assertEqual(after.get('name'),'肯萨')
        if latest.exists():
            before.set('name','肯萨')
            self.assertEqual(ET.tostring(before),ET.tostring(after))
        self.assertEqual([ET.tostring(x) for x in before], [ET.tostring(x) for x in after])
        binding = next(x for x in after.findall('TONE_PARAMETER') if x.get('id') == 'index:1:percent')
        self.assertAlmostEqual(float(binding.get('value')), (100-10)/390, places=6)
        self.assertFalse(audit['actualPitchEndpointsVerified'])

    def test_special_range_control_is_only_for_kensa_and_never_pitch_audio_processing(self):
        host = (ROOT/'native/Source/PluginHostEngine.cpp').read_text(encoding='utf-8')
        setter = host.split('bool PluginHostEngine::setBendRange(int semitones)', 1)[1].split('\nbool ', 1)[0]
        self.assertIn('if(kontaktStandardControls.load())', setter)
        self.assertIn('KensaBendRange::boundParameter(*plugin)', setter)
        self.assertIn('pitchBendChanged(0.0f)', setter)
        self.assertIn('KensaBendRange::set(*plugin,semitones)', setter)
        main = (ROOT/'native/Source/MainComponent.cpp').read_text(encoding='utf-8')
        self.assertIn('currentInstrumentKey=="kontakt-kensa"', main)
        helper = (ROOT/'native/Source/KensaBendRange.h').read_text(encoding='utf-8')
        self.assertIn('parameters[1]', helper)
        self.assertIn('equalsIgnoreCase("percent")', helper)
        for forbidden in ['setStateInformation', 'controllerEvent', 'processBlock']:
            self.assertNotIn(forbidden, helper)


if __name__ == '__main__':
    unittest.main()
