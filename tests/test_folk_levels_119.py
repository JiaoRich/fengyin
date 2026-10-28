from pathlib import Path
import unittest,json,zipfile,hashlib,io
import xml.etree.ElementTree as ET
import sys
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'scripts'))
from apply_swam_gain_curve_119 import decode
ROOT=Path(__file__).resolve().parents[1]
class FolkLevels119(unittest.TestCase):
    def test_all_thirteen_native_levels_preserve_host_and_technique_settings(self):
        audit=json.loads((ROOT/'assets/tone-packages/folk-engine-levels-119-audit.json').read_text(encoding='utf-8'))
        self.assertEqual(audit['nativeGain'],6);self.assertEqual(audit['nativeSlotFXAUX'],50)
        self.assertEqual(len(audit['targets']),13)
        migration=json.loads((ROOT/'assets/tone-packages/kong-dual-slot-133-audit.json').read_text(encoding='utf-8'))
        migrated={row['file']:row for row in migration['targets']}
        gain=json.loads((ROOT/'assets/tone-packages/kong-headroom-134-audit.json').read_text(encoding='utf-8'))
        calibrated={row['file']:row for row in gain['targets']}
        with zipfile.ZipFile(ROOT/audit['baseline']) as base:
            for row in audit['targets']:
                path=ROOT/'assets/tone-packages'/row['file']
                raw=path.read_bytes()
                newer=calibrated[row['file']]
                self.assertEqual(hashlib.sha256(raw).hexdigest(),newer['beforeSha256'])
                with zipfile.ZipFile(ROOT/gain['baseline']) as previous:raw=previous.read(row['file'])
                self.assertEqual(hashlib.sha256(raw).hexdigest(),newer['beforeSha256'])
                if row['file'] in migrated:
                    newer=migrated[row['file']]
                    self.assertEqual(hashlib.sha256(raw).hexdigest(),newer['afterSha256'])
                    raw=(ROOT/newer['baseline']).read_bytes()
                    self.assertEqual(hashlib.sha256(raw).hexdigest(),newer['beforeSha256'])
                self.assertEqual(hashlib.sha256(raw).hexdigest(),row['afterSha256'])
                self.assertEqual(hashlib.sha256(base.read(row['file'])).hexdigest(),row['beforeSha256'])
                with zipfile.ZipFile(io.BytesIO(base.read(row['file']))) as z:a=ET.fromstring(z.read('tone.xml'))
                with zipfile.ZipFile(io.BytesIO(raw)) as z:
                    self.assertIsNone(z.testzip());b=ET.fromstring(z.read('tone.xml'))
                    self.assertEqual(z.read('instrument.KAM'),decode(b.findtext('PRESET/KAM_PROJECT')))
                    for item in json.loads(z.read('manifest.json'))['files']:
                        data=z.read(item['path']);self.assertEqual(len(data),item['bytes']);self.assertEqual(hashlib.sha256(data).hexdigest(),item['sha256'])
                for tag in ['SAMPLER_STATE','KAM_PROJECT']:
                    self.assertIn('-> 6;',row['nativeVerification'][tag]);self.assertIn('-> 50',row['nativeVerification'][tag])
                    b.find('PRESET/'+tag).text=a.findtext('PRESET/'+tag)
                self.assertEqual(ET.tostring(a),ET.tostring(b))
if __name__=='__main__':unittest.main()
