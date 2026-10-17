import importlib.util,json,math,unittest,zipfile
import xml.etree.ElementTree as ET
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('gain119',ROOT/'scripts/apply_swam_gain_curve_119.py')
m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)

class SwamGainCurve119(unittest.TestCase):
    def test_all_nineteen_packages_only_approved_changes(self):
        audit=json.loads(m.AUDIT.read_text(encoding='utf-8'))
        self.assertEqual(len(audit['targets']),19)
        with zipfile.ZipFile(m.BASELINE) as baseline:
            for row in audit['targets']:
                with self.subTest(row['file']),zipfile.ZipFile(m.ASSETS/row['file']) as z:
                    self.assertIsNone(z.testzip())
                    old=ET.fromstring(zipfile.ZipFile(__import__('io').BytesIO(baseline.read(row['file']))).read('tone.xml'))
                    new=ET.fromstring(z.read('tone.xml'))
                    a,b=old.find('PRESET'),new.find('PRESET')
                    self.assertAlmostEqual(20*math.log10(float(b.get('outputGain'))),5,places=10)
                    a.set('outputGain',b.get('outputGain'))
                    if row['file']=='violin-soft.fytonepack':
                        s,t=a.findtext('INSTRUMENT_STATE'),b.findtext('INSTRUMENT_STATE')
                        oldcurve,x=m.mainvolume_curve(s);curve,y=m.mainvolume_curve(t)
                        self.assertEqual(float(curve.get('out_min')),82)
                        self.assertEqual(float(curve.get('out_max')),127)
                        entry=next(e for e in x.iter('MIDIRemappingEntry') if e.get('parameterId')=='mainVolume')
                        entry.remove(oldcurve);entry.append(ET.fromstring(ET.tostring(curve)))
                        self.assertEqual(ET.tostring(x),ET.tostring(y),'Non-volume SWAM parameter changed')
                        a.find('INSTRUMENT_STATE').text=t
                    self.assertEqual(ET.tostring(old),ET.tostring(new),'Unexpected preset/effect/snapshot change')
                    for entry in json.loads(z.read('manifest.json'))['files']:
                        data=z.read(entry['path'])
                        self.assertEqual(m.sha(data),entry['sha256']);self.assertEqual(len(data),entry['bytes'])
                    self.assertEqual(m.sha((m.ASSETS/row['file']).read_bytes()),row['afterSha256'])

    def test_strings_tremolo_and_attack_preserved(self):
        with zipfile.ZipFile(m.BASELINE) as base:
            for name in ['violin-soft.fytonepack','violin-bright.fytonepack','cello.fytonepack']:
                with zipfile.ZipFile(__import__('io').BytesIO(base.read(name))) as z:old=ET.fromstring(z.read('tone.xml')).find('PRESET')
                with zipfile.ZipFile(m.ASSETS/name) as z:new=ET.fromstring(z.read('tone.xml')).find('PRESET')
                _,a=m.mainvolume_curve(old.findtext('INSTRUMENT_STATE'));_,b=m.mainvolume_curve(new.findtext('INSTRUMENT_STATE'))
                for tag,key in [('PARAM','id'),('MIDIRemappingEntry','parameterId')]:
                    def select(r):return {e.get(key):ET.tostring(e) for e in r.iter(tag) if any(s in e.get(key,'').lower() for s in ['tremolo','attack','vibrato'])}
                    self.assertEqual(select(a),select(b),name)

if __name__=='__main__':unittest.main()
