"""Approved release mapping, exact source bytes, and build-embedded package coverage."""
import hashlib
import json
import re
import unittest
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]

class ToneRelease137(unittest.TestCase):
    def test_catalogue_matches_approved_package_inventory(self):
        audit = json.loads((ROOT/'assets/tone-packages/release-140-audit.json').read_text(encoding='utf-8'))
        self.assertEqual(audit['calibration'], 'deferred')
        rows = audit['tones']
        self.assertEqual(len(rows), 41)
        self.assertEqual(len({r['name'] for r in rows}), 41)
        source = (ROOT/'native/Source/PublishedToneCatalog.h').read_text(encoding='utf-8')
        actual = re.findall(r'\{ "([^"]+)",\s*"([^"]+)",\s*"([^"]+)",\s*"[^"]+",\s*PublishedToneKind::\w+,\s*"([^"]+)"', source)
        self.assertEqual(sorted(actual), sorted((r['id'],r['name'],r['instrumentKey'],r['asset']) for r in rows))  # Later releases may reorder defaults without changing tone state.
        cmake = (ROOT/'CMakeLists.txt').read_text(encoding='utf-8')
        embedded = re.findall(r'assets/tone-packages/([^\s]+\.fytonepack)',cmake)
        self.assertEqual(set(embedded), {r['asset'] for r in rows})
        for row in rows:
            with self.subTest(tone=row['name']):
                raw=(ROOT/'assets/tone-packages'/row['asset']).read_bytes()
                self.assertEqual(hashlib.sha256(raw).hexdigest(),row['sha256'])
        names={r['name'] for r in rows}
        self.assertTrue({'梆笛*','曲笛2*','唢呐2*','小号1','小号2','中萨-丝滑','次中萨-丝滑'} <= names)
        self.assertTrue({'小提琴-明亮','马头琴','葫芦丝'}.isdisjoint(names))
        for ident in ['kong-xun','kong-nanxiao','kong-dongxiao']:
            row=next(r for r in rows if r['id']==ident)
            self.assertTrue(row['asset'].startswith('tone137-'))
        self.assertEqual(sum(r['brand']=='swam' for r in rows),21)
        self.assertEqual(sum(r['brand']=='kong' for r in rows),19)
        self.assertEqual(sum(r['brand']=='kontakt' for r in rows),1)

if __name__=='__main__': unittest.main()
