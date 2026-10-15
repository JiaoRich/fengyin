import hashlib
import json
import re
import unittest
import zipfile
from pathlib import Path
import xml.etree.ElementTree as ET

ROOT=Path(__file__).resolve().parents[1]

class FolkPackages(unittest.TestCase):
    def test_all_published_folk_use_valid_routed_packages(self):
        catalog=(ROOT/'native/Source/PublishedToneCatalog.h').read_text(encoding='utf-8')
        names=re.findall(r'PublishedToneKind::kongPackage, "([^"]+)"',catalog)
        self.assertEqual(len(names),13)
        for name in names:
            with self.subTest(name=name),zipfile.ZipFile(ROOT/'assets/tone-packages'/name) as package:
                self.assertIsNone(package.testzip())
                manifest=json.loads(package.read('manifest.json'))
                self.assertEqual(manifest['version'],2)
                for item in manifest['files']:
                    payload=package.read(item['path'])
                    self.assertEqual(len(payload),item['bytes'])
                    self.assertEqual(hashlib.sha256(payload).hexdigest(),item['sha256'])
                preset=ET.fromstring(package.read('tone.xml')).find('PRESET')
                self.assertEqual(preset.get('kongTechniqueProfile'),'1')
                self.assertEqual(preset.get('pluginBrand'),'kong')
                self.assertTrue(preset.find('SAMPLER_STATE').text)
                self.assertTrue(preset.find('KAM_PROJECT').text)

if __name__=='__main__':unittest.main()
