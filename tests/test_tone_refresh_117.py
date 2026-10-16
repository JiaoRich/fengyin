import hashlib
import json
from pathlib import Path
import unittest
import zipfile
import xml.etree.ElementTree as ET

ROOT=Path(__file__).resolve().parents[1]

def preset(path):
    with zipfile.ZipFile(path) as z:
        return ET.fromstring(z.read('tone.xml')).find('PRESET')

class ToneRefresh117(unittest.TestCase):
    def test_soprano_is_the_exact_new_package(self):
        source=ROOT/'assets/incoming-tones/20261017/高音萨克斯-高萨-原声.fytonepack'
        asset=ROOT/'assets/tone-packages/soprano-kenny.fytonepack'
        self.assertEqual(source.read_bytes(),asset.read_bytes())
        attack=next(x for x in preset(asset).iter('TONE_PARAMETER') if x.get('id','').endswith(':attackcontrol'))
        self.assertEqual(float(attack.get('value')),1)

    def test_bright_only_changes_name_and_approved_attack_snapshot(self):
        source=preset(ROOT/'assets/incoming-tones/20261017/小提琴-明亮.fytonepack')
        new=preset(ROOT/'assets/tone-packages/violin-bright.fytonepack')
        source.set('name','小提琴-明亮')
        attack=next(x for x in source.iter('TONE_PARAMETER') if x.get('id','').endswith(':attackcontrol'))
        self.assertEqual(float(attack.get('value')),.5)
        attack.set('value','0.6666666865348816')
        self.assertEqual(ET.tostring(source),ET.tostring(new))
        self.assertEqual(float(new.get('outputGain')),1)

    def test_old_passion_is_not_shipped_and_soft_keeps_trim(self):
        self.assertFalse((ROOT/'assets/tone-packages/violin-sweet.fytonepack').exists())
        source=(ROOT/'native/Source/PublishedToneCatalog.h').read_text(encoding='utf-8')
        self.assertNotIn('激情小提琴',source)
        self.assertIn('"violin-bright",',source)
        soft=preset(ROOT/'assets/tone-packages/violin-soft.fytonepack')
        self.assertEqual(soft.get('name'),'小提琴-柔美')
        self.assertAlmostEqual(float(soft.get('outputGain')),10**.3,places=6)

    def test_new_names_have_exact_matching_offline_voices(self):
        catalog=json.loads((ROOT/'prototype/voice-catalog.js').read_text(encoding='utf-8').split('=',1)[1].strip().rstrip(';'))
        entries={x['id']:x for x in catalog['items']}
        self.assertNotIn('TONE_VIOLIN_SWEET',entries)
        for ident,text in [('V014_01','高音萨克斯，原声'),('TONE_VIOLIN_BRIGHT','小提琴，明亮'),('TONE_VIOLIN_SOFT','小提琴，柔美')]:
            self.assertEqual(entries[ident]['text'].rstrip('。'),text)
            self.assertEqual(entries[ident]['speechText'],text)
            self.assertEqual(entries[ident]['voice'],'x6_wuyediantai_mini')

if __name__=='__main__':unittest.main()
