from pathlib import Path
import hashlib
import io
import json
import math
import re
import subprocess
import sys
import unittest
import zipfile

ROOT = Path(__file__).resolve().parents[1]
ASSETS = ROOT / 'assets/tone-packages'


def sha(data):
    return hashlib.sha256(data).hexdigest()


class KongHeadroom134(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.audit = json.loads((ASSETS / 'kong-headroom-134-audit.json').read_text(encoding='utf-8'))

    def test_only_authorized_output_gain_changed(self):
        audit = self.audit
        self.assertTrue(audit['userAuthorized'])
        self.assertEqual({r['file'] for r in audit['targets']}, {p.name for p in ASSETS.glob('folk-*.fytonepack')})
        self.assertEqual(len(audit['targets']), 13)
        with zipfile.ZipFile(ROOT / audit['baseline']) as previous:
            for row in audit['targets']:
                original = previous.read(row['file'])
                self.assertEqual(sha(original), row['beforeSha256'])
                current = (ASSETS / row['file']).read_bytes()
                self.assertEqual(sha(current), row['afterSha256'])
                with zipfile.ZipFile(io.BytesIO(original)) as before, zipfile.ZipFile(io.BytesIO(current)) as after:
                    self.assertIsNone(after.testzip())
                    self.assertEqual(before.namelist(), after.namelist())
                    a, b = before.read('tone.xml'), after.read('tone.xml')
                    self.assertEqual(len(re.findall(rb'\boutputGain="', b)), 1)
                    restored = b.replace(('outputGain="' + row['afterGainText'] + '"').encode(),
                                         ('outputGain="' + row['beforeGainText'] + '"').encode(), 1)
                    self.assertEqual(restored, a, row['file'])
                    self.assertGreater(float(row['afterGainText']), 0)
                    self.assertLessEqual(float(row['afterGainText']), float(row['beforeGainText']))
                    self.assertAlmostEqual(row['gainDeltaDb'], 20 * math.log10(float(row['afterGainText']) / float(row['beforeGainText'])))
                    for name in before.namelist():
                        if name not in ('tone.xml', 'manifest.json'):
                            self.assertEqual(before.read(name), after.read(name), row['file'])
                    for entry in json.loads(after.read('manifest.json'))['files']:
                        data = after.read(entry['path'])
                        self.assertEqual(len(data), entry['bytes'])
                        self.assertEqual(sha(data), entry['sha256'])

    def test_native_measurements_are_bound_to_current_packages(self):
        self.assertEqual(self.audit['performanceFaderTested'], 1.5)
        for row in self.audit['targets']:
            for rate in ('44100', '48000'):
                evidence = row['verification'][rate]
                self.assertEqual(evidence['packageSha256'], row['afterSha256'])
                self.assertEqual(evidence['protocol'], 2)
                self.assertTrue(evidence['finite'])
                self.assertEqual(evidence['droppedMidi'], 0)
                self.assertEqual(evidence['overloadSamples'], 0)
                self.assertEqual(evidence['cases'], row['cases'])
                self.assertGreater(evidence['fullExpressionAttackCases'], 0)
                self.assertEqual(evidence['longToneCases'], 3)
                if evidence['dual']:
                    self.assertGreater(evidence['preHeldTechniqueAttackCases'], 0)
                self.assertGreater(evidence['peak'], 0)
                self.assertLessEqual(max(evidence['peak'], evidence['oversampledPeak4x']), self.audit['verificationCeiling'])

    def test_current_exclusive_signal_path_and_other_builtins(self):
        maintenance = json.loads((ASSETS / 'kong-exclusive-136-audit.json').read_text(encoding='utf-8'))
        self.assertEqual(maintenance['sourceHashNewlines'], 'LF')
        for path, expected in maintenance['signalPathSha256'].items():
            # Git's Windows checkout may convert LF to CRLF. Ignore only that
            # code-line ending conversion; package/binary hashes remain exact.
            source=(ROOT / path).read_bytes().replace(b'\r\n',b'\n')
            self.assertEqual(sha(source), expected, path)
        for name, expected in self.audit['unchangedPackages'].items():
            self.assertFalse(name.startswith('folk-'))
            self.assertEqual(sha((ASSETS / name).read_bytes()), expected, name)

    def test_exclusive_native_measurements(self):
        maintenance = json.loads((ASSETS / 'kong-exclusive-136-audit.json').read_text(encoding='utf-8'))
        self.assertEqual({r['file'] for r in maintenance['targets']}, {r['file'] for r in self.audit['targets']})
        for row in maintenance['targets']:
            self.assertEqual(set(row['verification']), {'44100', '48000'})
            for rate, v in row['verification'].items():
                self.assertEqual(v['packageSha256'], sha((ASSETS / row['file']).read_bytes()))
                self.assertEqual(v['sampleRate'], int(rate))
                self.assertEqual(v['protocol'], 2)
                self.assertEqual(v['fader'], 1.5)
                self.assertTrue(v['finite'])
                self.assertEqual(v['droppedMidi'], 0)
                self.assertEqual(v['overloadSamples'], 0)
                self.assertGreater(v['cases'], 250)
                self.assertGreater(v['peak'], 0)
                self.assertLessEqual(max(v['peak'], v['oversampledPeak4x']), maintenance['verificationCeiling'])

    def test_calibration_cannot_accumulate_on_repeat(self):
        paths = list(ASSETS.glob('*.fytonepack'))
        before = {p.name: sha(p.read_bytes()) for p in paths}
        subprocess.run([sys.executable, str(ROOT / 'scripts/apply_kong_headroom_134.py'),
                        '--measurements', 'unused-after-completed-calibration.json'], check=True, capture_output=True)
        self.assertEqual(before, {p.name: sha(p.read_bytes()) for p in paths})


if __name__ == '__main__':
    unittest.main()
