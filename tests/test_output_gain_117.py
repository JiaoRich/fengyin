import hashlib
import json
import math
from pathlib import Path
import unittest
import zipfile
from io import BytesIO
from tone_baseline_118 import historical_bytes

ROOT = Path(__file__).resolve().parents[1]


class FixedOutputGain117(unittest.TestCase):
    def test_studio_uses_the_full_persisted_gain_range(self):
        source=(ROOT/'native/Source/MainComponent.cpp').read_text(encoding='utf-8')
        self.assertIn('toneSettings.outputGain * 50.0f',source)
        self.assertIn('normal("output", result.outputGain / 2.0) * 2.0',source)
        self.assertNotIn('0.5 + normal("output", 0.8) * 0.75',source)

    def test_exact_six_db_and_no_plugin_or_curve_changes(self):
        audit = json.loads((ROOT/'assets/tone-packages/output-gain-117-audit.json').read_text(encoding='utf-8'))
        expected = {'flute', 'piccolo', 'clarinet', 'oboe', 'violin-soft'} | {
            'folk-'+x for x in ('erhu','hulusi','liuqin','matouqin','qudi','xun','suona',
                               'sanxian','pipa','nanxiao','dongxiao','bangdi','jinghu')}
        self.assertEqual({Path(x['file']).stem for x in audit['targets']}, expected)
        for row in audit['targets']:
            path = ROOT/'assets/tone-packages'/row['file']
            self.assertEqual(hashlib.sha256(historical_bytes(path)).hexdigest(), row['afterPackageSha256'])
            self.assertAlmostEqual(20*math.log10(float(row['afterGainText'])/float(row['beforeGainText'])),6,places=10)
            with zipfile.ZipFile(BytesIO(historical_bytes(path))) as z:
                self.assertIsNone(z.testzip())
                tone = z.read('tone.xml')
                restored = tone.replace(('outputGain="'+row['afterGainText']+'"').encode(),
                                        ('outputGain="'+row['beforeGainText']+'"').encode())
                if row.get('displayRename'):
                    rename=row['displayRename']
                    restored=restored.replace(('name="'+rename['after']+'"').encode(),('name="'+rename['before']+'"').encode())
                self.assertNotEqual(tone,restored)
                self.assertEqual(hashlib.sha256(restored).hexdigest(),row['beforeToneSha256'])
                for name,digest in row['preservedEntries'].items():
                    self.assertEqual(hashlib.sha256(z.read(name)).hexdigest(),digest)
                for entry in json.loads(z.read('manifest.json'))['files']:
                    data=z.read(entry['path'])
                    self.assertEqual(len(data),entry['bytes'])
                    self.assertEqual(hashlib.sha256(data).hexdigest(),entry['sha256'])


if __name__ == '__main__':
    unittest.main()
