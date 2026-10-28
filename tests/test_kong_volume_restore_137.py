from pathlib import Path
import hashlib
import io
import json
import subprocess
import sys
import unittest
import zipfile

ROOT = Path(__file__).resolve().parents[1]
ASSETS = ROOT / 'assets/tone-packages'


def sha(data):
    return hashlib.sha256(data).hexdigest()


class OriginalKongVolume(unittest.TestCase):
    def test_exact_original_volume_and_no_other_tuning_changes(self):
        audit = json.loads((ASSETS / 'kong-volume-restore-137-audit.json').read_text(encoding='utf-8'))
        self.assertEqual({r['file'] for r in audit['targets']}, {p.name for p in ASSETS.glob('folk-*.fytonepack')})
        with zipfile.ZipFile(ROOT / audit['restoredBaseline']) as base, zipfile.ZipFile(ROOT / audit['calibratedArchive']) as history:
            for row in audit['targets']:
                current = (ASSETS / row['file']).read_bytes()
                previous = history.read(row['file'])
                self.assertEqual(current, base.read(row['file']))
                self.assertEqual(sha(current), row['restoredSha256'])
                self.assertEqual(sha(previous), row['calibratedSha256'])
                with zipfile.ZipFile(io.BytesIO(previous)) as a, zipfile.ZipFile(io.BytesIO(current)) as b:
                    self.assertEqual(a.namelist(), b.namelist())
                    self.assertIsNone(b.testzip())
                    for name in a.namelist():
                        if name not in ('tone.xml', 'manifest.json'):
                            self.assertEqual(a.read(name), b.read(name))
                    expected = a.read('tone.xml').replace(('outputGain="'+row['calibratedGainText']+'"').encode(),
                                                        ('outputGain="'+row['restoredGainText']+'"').encode(), 1)
                    self.assertEqual(expected, b.read('tone.xml'))
                    for entry in json.loads(b.read('manifest.json'))['files']:
                        data = b.read(entry['path'])
                        self.assertEqual(len(data), entry['bytes'])
                        self.assertEqual(sha(data), entry['sha256'])

    def test_repeat_restore_and_retired_calibration_never_change_packages(self):
        paths = list(ASSETS.glob('*.fytonepack'))
        before = {p.name: sha(p.read_bytes()) for p in paths}
        subprocess.run([sys.executable, str(ROOT / 'scripts/restore_kong_gain_137.py')], check=True, capture_output=True)
        result = subprocess.run([sys.executable, str(ROOT / 'scripts/apply_kong_headroom_134.py'), '--measurements', 'unused', '--refine'], capture_output=True)
        self.assertNotEqual(result.returncode, 0)
        self.assertEqual(before, {p.name: sha(p.read_bytes()) for p in paths})


if __name__ == '__main__':
    unittest.main()
