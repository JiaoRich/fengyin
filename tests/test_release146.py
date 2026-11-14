"""Accepted tuning must survive both full-state and parameter-snapshot restore."""
import hashlib
import importlib.util
from pathlib import Path
import unittest
import xml.etree.ElementTree as ET
from zipfile import ZipFile

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('swam_state_codec', ROOT/'scripts/apply_swam_gain_curve_119.py')
codec = importlib.util.module_from_spec(spec)
spec.loader.exec_module(codec)


class Release146Tuning(unittest.TestCase):
    def preset(self, name):
        with ZipFile(ROOT/'assets/tone-packages'/name) as package:
            self.assertIsNone(package.testzip())
            return ET.fromstring(package.read('tone.xml')).find('PRESET')

    def test_trombone_native_and_snapshot_agree_on_valves(self):
        for name in ('alto-trombone.fytonepack', 'bass-trombone.fytonepack'):
            with self.subTest(package=name):
                preset = self.preset(name)
                outer = ET.fromstring(codec.xml_bytes(codec.decode(preset.findtext('INSTRUMENT_STATE'))))
                native = ET.fromstring(codec.xml_bytes(codec.decode(outer.findtext('IComponent'))[176:]))
                params = {p.get('id'): p.get('value') for p in native.iter('PARAM')}
                snapshot = {p.get('id'): p.get('value') for p in preset.iter('TONE_PARAMETER')}
                self.assertEqual(float(params['pipeModel']), 0.0)  # SWAM native enum: Valves.
                self.assertEqual(float(snapshot['index:9:pipemodel']), 0.0)
                self.assertAlmostEqual(float(params['portamentoMaxTime']), 2.4, places=5)
                self.assertEqual(float(params['attackControl']), 2.0)  # Brass Expression retained.
                self.assertEqual(float(snapshot['index:88:attackcontrol']), 1.0)

    def test_kensa_keeps_accepted_complete_kontakt_state(self):
        preset = self.preset('kontakt-kensa.fytonepack')
        self.assertEqual(preset.get('name'), '肯萨')
        self.assertEqual(preset.get('instrumentKey'), 'kontakt-kensa')
        self.assertEqual(preset.findall('SERIAL_EFFECT'), [])
        # Accepted 1115 export, after live user verification of soft onset/legato.
        # Host display name and unused bypassed effect are outside this state.
        digest = hashlib.sha256(codec.decode(preset.findtext('INSTRUMENT_STATE'))).hexdigest()
        self.assertEqual(digest, '29015bd719f0b21c18cb4070a82d175dced937748d2af60ea43b77a64e0cbb19')


if __name__ == '__main__':
    unittest.main()
