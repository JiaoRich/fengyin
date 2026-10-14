import hashlib,json,re,unittest,zipfile
from pathlib import Path
import xml.etree.ElementTree as ET
ROOT=Path(__file__).resolve().parents[1]
class AdjustedPackages(unittest.TestCase):
    def test_manifest_and_approved_attack(self):
        audit=json.loads((ROOT/'assets/tone-packages/adjusted-112-audit.json').read_text(encoding='utf-8'))
        self.assertEqual(len(audit),16)
        self.assertEqual([r['id'] for r in audit if r['attackChangedWithApproval']],['tenor-air-pocket'])
        for row in audit:
            with zipfile.ZipFile(ROOT/'assets/tone-packages'/f"{row['id']}.fytonepack") as z:
                self.assertIsNone(z.testzip())
                for record in json.loads(z.read('manifest.json'))['files']:
                    payload=z.read(record['path']);self.assertEqual(len(payload),record['bytes'])
                    self.assertEqual(hashlib.sha256(payload).hexdigest(),record['sha256'])
                preset=ET.fromstring(z.read('tone.xml')).find('PRESET')
                self.assertTrue(preset.find('INSTRUMENT_STATE').text)
                attack=next(p for p in preset.iter('TONE_PARAMETER') if p.get('id','').endswith(':attackcontrol'))
                self.assertAlmostEqual(float(attack.get('value')),row['attackExpression'],places=6)
    def test_catalogue_retirement(self):
        text=(ROOT/'native/Source/PublishedToneCatalog.h').read_text(encoding='utf-8')
        self.assertEqual(len(re.findall(r'\{ "',text)),31)
        for ident in ('baritone-jazz','baritone-pop','bassoon','viola','violin-section','viola-section','cello-section'):
            self.assertNotIn(f'"{ident}"',text)
        self.assertIn('"甜美小提琴"',text);self.assertIn('"柔美小提琴"',text);self.assertIn('"大提琴"',text)
    def test_final_routing_after_restore_and_kontakt_not_kam(self):
        main=(ROOT/'native/Source/MainComponent.cpp').read_text(encoding='utf-8')
        loaded=main.split('void MainComponent::completeLoadedPreset',1)[1].split('void MainComponent::activatePluginOutput',1)[0]
        self.assertLess(loaded.index('restoreToneParameters'),loaded.index('applyStandardSwamExpressionCurve'))
        package=(ROOT/'native/Source/TonePackage.h').read_text(encoding='utf-8')
        self.assertIn('preset.pluginBrand == "kontakt" && preset.containerInstrument',package)
        classifier=(ROOT/'native/Source/KongInstrumentCatalog.h').read_text(encoding='utf-8')
        self.assertIn('return Brand::kontakt',classifier)
if __name__=='__main__':unittest.main()
