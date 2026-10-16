import hashlib
import json
from pathlib import Path
import unittest
import xml.etree.ElementTree as ET
import zipfile

ROOT = Path(__file__).resolve().parents[1]

class AttackControlPolicyTests(unittest.TestCase):
    def test_bundled_packages_and_checksums(self):
        packages = list((ROOT / 'assets/tone-packages').glob('*.fytonepack'))
        self.assertTrue(packages)
        for path in packages:
            with self.subTest(package=path.name), zipfile.ZipFile(path) as archive:
                self.assertIsNone(archive.testzip())
                xml = archive.read('tone.xml')
                controls = [p for p in ET.fromstring(xml).iter('TONE_PARAMETER')
                            if p.get('id', '').endswith(':attackcontrol')]
                preset=ET.fromstring(xml).find('PRESET')
                if preset.get('pluginBrand')=='swam':
                    self.assertEqual(len(controls), 1)
                    # Verified Windows enums differ between saxophones and strings.
                    expected=0.666666686534882 if preset.get('instrumentKey') in ('violin','cello') else 1.0
                    self.assertAlmostEqual(float(controls[0].get('value')),expected,places=6)
                else:
                    self.assertIn(preset.get('pluginBrand'),['kong','kontakt'])
                    self.assertEqual(controls,[])  # SWAM enums must not be invented for Qin.
                manifest = json.loads(archive.read('manifest.json'))
                for entry in manifest['files']:
                    payload = archive.read(entry['path'])
                    self.assertEqual(len(payload), entry['bytes'])
                    self.assertEqual(hashlib.sha256(payload).hexdigest(), entry['sha256'])

    def test_runtime_resolves_label_not_index(self):
        source = (ROOT / 'native/Source/PluginHostEngine.cpp').read_text(encoding='utf-8')
        body = source.split('bool PluginHostEngine::applyExpressionAttackControl()')[1].split(
            'bool PluginHostEngine::applyStandardSwamExpressionCurve()')[0]
        self.assertIn('ScopedGraphPause', body)
        self.assertIn('normalisedParameterName(*parameter) != "attackcontrol"', body)
        self.assertIn('getText(parameter->getValue(), 128)', body)
        self.assertNotIn('index:78', body)
        self.assertIn('equalsIgnoreCase("Expression")', body)

if __name__ == '__main__':
    unittest.main()
