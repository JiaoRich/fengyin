import hashlib, importlib.util, io, json, sys, unittest, zipfile
from pathlib import Path
import xml.etree.ElementTree as ET
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'scripts'))
from prepare_release_139 import patch_transpose
class Release139(unittest.TestCase):
    def test_only_factory_transpose_changed_in_trumpet(self):
        with zipfile.ZipFile(ROOT/'assets/incoming-tones/20261018/swam-post-gain-pre-139.zip') as z:oldBytes=z.read('trumpet.fytonepack')
        with zipfile.ZipFile(io.BytesIO(oldBytes)) as z:old=ET.fromstring(z.read('tone.xml'));oldFiles=set(z.namelist())
        with zipfile.ZipFile(ROOT/'assets/tone-packages/trumpet.fytonepack') as z:
            new=ET.fromstring(z.read('tone.xml'));self.assertEqual(oldFiles,set(z.namelist()));self.assertIsNone(z.testzip())
            for item in json.loads(z.read('manifest.json'))['files']:
                self.assertEqual(hashlib.sha256(z.read(item['path'])).hexdigest(),item['sha256'])
        state=old.find('PRESET/INSTRUMENT_STATE');state.text=patch_transpose(state.text)
        self.assertEqual(ET.tostring(old),ET.tostring(new),'Unexpected tuner/effect/mapping change')
    def test_all_other_package_bytes_unchanged_and_only_kensa2_removed(self):
        old=json.loads((ROOT/'assets/tone-packages/release-137-audit.json').read_text(encoding='utf-8'))['tones']
        new=json.loads((ROOT/'assets/tone-packages/release-139-audit.json').read_text(encoding='utf-8'))['tones']
        self.assertEqual({r['id'] for r in old}-{r['id'] for r in new},{'kontakt-kensa-2'})
        self.assertFalse((ROOT/'assets/tone-packages/tone137-kontakt-kensa-2.fytonepack').exists())
        for row in new:
            if row['id']=='trumpet':continue
            self.assertEqual(row['sha256'],next(r['sha256'] for r in old if r['id']==row['id']))

    def test_ble_address_type_keeps_older_windows_compatible(self):
        source=(ROOT/'native/Source/BluetoothMidiService.cpp').read_text(encoding='utf-8')
        self.assertIn('ApiInformation::IsPropertyPresent',source)
        self.assertIn('hasAddressType?static_cast<int>(args.BluetoothAddressType()):2',source)
        self.assertIn('if(type!=0 && type!=1)return Ble::FromBluetoothAddressAsync(address)',source)
