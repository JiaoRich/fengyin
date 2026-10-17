import unittest
from pathlib import Path
import zipfile
import xml.etree.ElementTree as ET
from tone_baseline_118 import historical_bytes
ROOT=Path(__file__).resolve().parents[1]
class KensaRange117(unittest.TestCase):
    def test_range_is_kensa_only_and_preserves_off(self):
        s=(ROOT/'native/Source/PluginHostEngine.cpp').read_text(encoding='utf-8')
        route=s.split('if (kontaktStandardControls.load()) {',1)[1].split('if (kongExpressionMode.load',1)[0]
        self.assertIn('kensaVibratoController(safeValue)',route)
        helper=(ROOT/'native/Source/KensaVibratoRange.h').read_text(encoding='utf-8')
        self.assertIn('depth<=0.f',helper)
        self.assertIn(',31,70)',helper)
        self.assertNotIn('controllerEvent(1,19,',route)
    def test_soprano_bright_is_user_package_unchanged(self):
        source=ROOT/'assets/incoming-tones/20261017/高音萨克斯-高萨-明亮.fytonepack'
        target=ROOT/'assets/tone-packages/soprano-pop.fytonepack'
        self.assertEqual(source.read_bytes(),historical_bytes(target))
        with zipfile.ZipFile(target) as z:
            self.assertIsNone(z.testzip())
            p=ET.fromstring(z.read('tone.xml')).find('PRESET')
        self.assertEqual(p.get('name'),'高萨-明亮')
        self.assertEqual(float(next(x for x in p.iter('TONE_PARAMETER') if x.get('id','').endswith(':attackcontrol')).get('value')),1)
if __name__=='__main__':unittest.main()
