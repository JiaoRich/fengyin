"""DJ data fidelity, independent install identity and control-chain boundaries."""
import hashlib, importlib.util, json, re, unittest, zipfile
from pathlib import Path
import xml.etree.ElementTree as E
R=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('codec',R/'scripts/apply_swam_gain_curve_119.py')
codec=importlib.util.module_from_spec(spec);spec.loader.exec_module(codec)
sha=lambda b:hashlib.sha256(b).hexdigest()
from test_release_147 import restore_native
CURRENT={r['id']:r for r in json.loads((R/'assets/tone-packages/release-147-audit.json').read_text(encoding='utf-8'))['rows']}
class DjRelease(unittest.TestCase):
 def test_all_source_states_and_effects_are_complete(self):
  rows=json.loads((R/'assets/tone-packages/dj-source-audit.json').read_text(encoding='utf-8'))
  self.assertEqual(len(rows),29);self.assertEqual(len({r['id'] for r in rows}),29)
  self.assertEqual(sum(len(r['effects'])==2 for r in rows),8)
  for row in rows:
   with self.subTest(tone=row['name']),zipfile.ZipFile(R/'assets/tone-packages'/row['asset']) as z:
    self.assertIsNone(z.testzip());self.assertEqual(sha((R/'assets/tone-packages'/row['asset']).read_bytes()),CURRENT.get(row['id'],{}).get('resultSHA',row['sha256']))
    manifest=json.loads(z.read('manifest.json'));self.assertEqual(manifest['version'],4)
    self.assertFalse(manifest['pluginBinariesIncluded'])
    p=E.fromstring(z.read('tone.xml')).find('PRESET');self.assertEqual(p.get('djSourceChain'),'1')
    for name,key in [('djTrackGain','trackGain'),('djHardwareGain','hardwareGain')]:self.assertAlmostEqual(float(p.get(name)),row[key])
    self.assertEqual(p.get('djDirectMaster')=='1',row['directMaster'])
    state=codec.decode(p.findtext('INSTRUMENT_STATE'))
    if row['id'] not in CURRENT:self.assertEqual(sha(state),row['packStateSHA'])
    else:self.assertEqual(CURRENT[row['id']]['sourceSHA'],row['sha256'])
    outer=E.fromstring(codec.xml_bytes(state));comp=codec.decode(outer.findtext('IComponent'));native=E.fromstring(restore_native(codec.xml_bytes(comp[176:]),CURRENT[row['id']]['changes']) if row['id'] in CURRENT else codec.xml_bytes(comp[176:]))
    attack=[x for x in native.iter('PARAM') if x.get('id','').lower() in ('attacktype','attackcontrol')]
    self.assertEqual(len(attack),1);self.assertEqual(float(attack[0].get('value')),0 if row['track'] in (2,3) else 2)
    if row['track']==15:
     table=next(x for x in native.iter() if any(c.tag=='MIDIRemappingEntry' for c in x))
     added=[x for x in table if x.get('parameterId')=='vibratoDepth'];self.assertEqual(len(added),1)
     self.assertEqual(added[0].get('msb'),'1');table.remove(added[0])
    else:self.assertEqual(row['sourceStateSHA'],row['packStateSHA'])
    self.assertEqual(sha(E.tostring(native)),row['nativeXmlSHA'])
    fx=p.findall('SERIAL_EFFECT');self.assertEqual(len(fx),len(row['effects']))
    for actual,expected in zip(fx,row['effects']):
     self.assertEqual(sha(codec.decode(actual.findtext('STATE'))),expected['sha256'])
     self.assertEqual(actual.get('bypassed'),'0')
     self.assertEqual(E.fromstring(actual.findtext('DESCRIPTION')).get('name'),expected['name'])
    self.assertEqual(row['directMaster'],row['track']==7)
 def test_separate_identity_and_original_packages(self):
  catalog=(R/'native/Source/PublishedToneCatalog.h').read_text(encoding='utf-8')
  names=re.findall(r'\{ "([^"]+)",\s*"([^"]+)",\s*"([^"]+)"',catalog)
  self.assertEqual(len(names),60)
  rows=json.loads((R/'assets/tone-packages/release-144-audit.json').read_text(encoding='utf-8'))['tones']
  self.assertEqual([n[0] for n in names if not n[0].startswith('dj-source-')],[r['id'] for r in CURRENT.values() if not r['id'].startswith('dj-source-')])
  changes={r['id']:r for r in json.loads((R/'assets/tone-packages/test-edition-audit.json').read_text(encoding='utf-8'))}
  for r in rows:self.assertEqual(changes[r['id']]['oldSHA'],r['sha256'])
  installer=(R/'installer/FengYin.iss').read_text(encoding='utf-8')
  self.assertIn('DefaultDirName={autopf}\\风吟',installer)
  self.assertIn('BA041761-FF1B-4F84-B055-AE70EC8C883B',installer)
  for f in ('MainComponent.cpp','MidiInputService.cpp','SoundPresetStore.cpp'):
   self.assertNotIn('FengYinDJ',(R/'native/Source'/f).read_text(encoding='utf-8'))
 def test_runtime_source_routes_cannot_use_standard_normalization(self):
  cpp=(R/'native/Source/PluginHostEngine.cpp').read_text(encoding='utf-8')
  body=cpp.split('bool PluginHostEngine::applyStandardSwamExpressionCurve')[1].split('// The player')[0]
  self.assertIn('if(usesDjSource())return configureDjSource(djSource) && rebuildConnections()',body)
  host=(R/'native/Source/MainComponent.cpp').read_text(encoding='utf-8')
  self.assertIn('!preset.djSource.enabled',host)
  self.assertIn('setSourceEffectsBypassed(preset.djSource.enabled)',host)
  self.assertIn('preset.djSource = pluginHost.getDjSource()',host)
  self.assertIn('editingReturnDjSource=pluginHost.getDjSource()',host)
if __name__=='__main__':unittest.main()
