"""Approved release mapping, exact source bytes, and build-embedded package coverage."""
import hashlib
import importlib.util
import json
import re
import unittest
from pathlib import Path
from xml.etree import ElementTree
from zipfile import ZipFile
ROOT = Path(__file__).resolve().parents[1]

class ToneRelease137(unittest.TestCase):
    def test_catalogue_matches_approved_package_inventory(self):
        audit = json.loads((ROOT/'assets/tone-packages/release-144-audit.json').read_text(encoding='utf-8'))
        self.assertEqual(audit['calibration'], 'deferred')
        rows = audit['tones']
        self.assertEqual(len(rows), 44)
        self.assertEqual(len({r['name'] for r in rows}), 44)
        source = (ROOT/'native/Source/PublishedToneCatalog.h').read_text(encoding='utf-8')
        actual = re.findall(r'\{ "([^"]+)",\s*"([^"]+)",\s*"([^"]+)",\s*"[^"]+",\s*PublishedToneKind::\w+,\s*"([^"]+)"', source)
        self.assertEqual([r for r in actual if not r[0].startswith('dj-source-')], [(r['id'], r['name'], r['instrumentKey'], r['asset']) for r in rows])
        cmake = (ROOT/'CMakeLists.txt').read_text(encoding='utf-8')
        embedded = re.findall(r'assets/tone-packages/([^\s]+\.fytonepack)',cmake)
        self.assertEqual({p for p in embedded if not p.startswith('dj-source-')}, {r['asset'] for r in rows})
        for row in rows:
            with self.subTest(tone=row['name']):
                changes=json.loads((ROOT/'assets/tone-packages/test-edition-audit.json').read_text(encoding='utf-8'))
                changed=next(r for r in changes if r['asset']==row['asset'])
                self.assertEqual(changed['oldSHA'],row['sha256'])
                raw=(ROOT/'assets/tone-packages'/row['asset']).read_bytes()
                self.assertEqual(hashlib.sha256(raw).hexdigest(),changed['sha256'])
        names={r['name'] for r in rows}
        self.assertTrue({'梆笛*','曲笛2*','唢呐2*','小号1','小号2','浪漫中萨','浪漫次中萨','嘶吼中萨','嘶吼次中萨','气声中萨'} <= names)
        self.assertTrue({'小提琴-明亮','马头琴','葫芦丝'}.isdisjoint(names))
        for ident in ['kong-xun','kong-nanxiao','kong-dongxiao']:
            row=next(r for r in rows if r['id']==ident)
            self.assertTrue(row['asset'].startswith('tone137-'))
        self.assertEqual(sum(r['brand']=='swam' for r in rows),24)
        self.assertEqual(sum(r['brand']=='kong' for r in rows),19)
        self.assertEqual(sum(r['brand']=='kontakt' for r in rows),1)

    def test_swam_sax_vibrato_curves_have_progressive_depth(self):
        assets = ROOT / 'assets/tone-packages'
        spec = importlib.util.spec_from_file_location(
            'swam_binary_xml', ROOT / 'scripts/apply_swam_gain_curve_119.py')
        codec = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(codec)
        rows = json.loads((assets / 'release-144-audit.json').read_text(encoding='utf-8'))['tones']
        saxes = [row for row in rows if row['brand'] == 'swam' and row['instrumentKey'].endswith('-sax')]
        self.assertEqual(len(saxes), 13)
        for row in saxes:
            with self.subTest(tone=row['name']), ZipFile(assets / row['asset']) as package:
                preset = ElementTree.fromstring(package.read('tone.xml')).find('PRESET')
                outer = ElementTree.fromstring(codec.xml_bytes(codec.decode(preset.findtext('INSTRUMENT_STATE'))))
                component = codec.decode(outer.findtext('IComponent'))
                native = ElementTree.fromstring(codec.xml_bytes(component[176:]))
                route, = [entry for entry in native.iter('MIDIRemappingEntry')
                          if entry.get('parameterId') == 'vibratoDepth']
                self.assertEqual((route.get('enabled'), route.get('messageType'), route.get('msb')), ('1', '1', '1'))
                low, high = (0.0, 45.0) if row['id'] == 'soprano-pop' else (1.0, 110.0)
                curve = route.find('MIDIRemappingCurve')
                expected = {'input_min': 0.0, 'input_max': 127.0, 'out_min': low, 'out_max': high,
                            'shape': 0.0, 'symmetry': 0.5, 'bypass': 0.0, 'bipolar': 0.0}
                self.assertEqual({key: float(curve.get(key)) for key in expected}, expected)

if __name__=='__main__': unittest.main()
