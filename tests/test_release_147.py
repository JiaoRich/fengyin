"""Formal 1.1.47: exact approved native-state changes, not just package hashes."""
import hashlib, json, re, unittest, zipfile, importlib.util, struct
from pathlib import Path
import xml.etree.ElementTree as E
R=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('codec147',R/'scripts/apply_swam_gain_curve_119.py');codec=importlib.util.module_from_spec(spec);spec.loader.exec_module(codec)
sha=lambda b:hashlib.sha256(b).hexdigest()
AUDIT=json.loads((R/'assets/tone-packages/release-147-audit.json').read_text(encoding='utf-8'))
def replace_attr(tag,key,value):
    result,n=re.subn(rb'\b'+key.encode()+rb'="[^"]*"',lambda _:key.encode()+b'="'+str(value).encode()+b'"',tag)
    assert n==1,(key,tag[:100]);return result
def restore_native(raw,changes):
    for c in reversed(changes):
        if 'parameter' in c:
            raw,n=re.subn(rb'<PARAM\b[^>]*\bid="'+c['parameter'].encode()+rb'"[^>]*/>',lambda m:replace_attr(m[0],'value',c['before']),raw)
        elif 'route' in c:
            def undo(m):
                b=m[0]
                for k,v in c['before'].items():b=replace_attr(b,k,v)
                return b
            raw,n=re.subn(rb'<MIDIRemappingEntry\b[^>]*\bparameterId="'+c['route'].encode()+rb'"[^>]*>',undo,raw)
        else:
            def undo(m):
                b=m[2]
                for k,v in c['before'].items():b=replace_attr(b,k,v)
                return m[1]+b
            raw,n=re.subn(rb'(<MIDIRemappingEntry\b[^>]*\bparameterId="'+c['curve'].encode()+rb'"[^>]*>\s*)(<MIDIRemappingCurve\b[^>]*/>)',undo,raw)
        assert n==1,c
    return raw
def restore_state(text, changes):
    outer=codec.decode(text);ox=codec.xml_bytes(outer);component=E.fromstring(ox).findtext('IComponent');body=codec.decode(component)
    chunk=codec.replace_xml(body[176:],restore_native(codec.xml_bytes(body[176:]),changes));newbody=bytearray(body[:176]+chunk)
    struct.pack_into('>I',newbody,20,len(newbody)-24);struct.pack_into('>I',newbody,172,len(chunk))
    return codec.encode(codec.replace_xml(outer,ox.replace(component.encode(),codec.encode(newbody).encode())))
class Release147(unittest.TestCase):
 def test_only_approved_native_state_changes_and_roundtrip(self):
  forms={'soprano-kenny':(78,127),'soprano-pop':(78,127),'alto-jazz':(85,115),'alto-deep':(39,127),'tone137-alto-silky':(39,127),'alto-breath-growl':(39,127),'tenor-mellow':(70,127),'tenor-breath-growl':(70,127),'tenor-warm':(35,127),'tone137-tenor-silky':(35,127)}
  # Stable IDs of the two historical packages differ from filenames.
  forms['alto-silky']=forms.pop('tone137-alto-silky');forms['tenor-silky']=forms.pop('tone137-tenor-silky')
  harmonics={'soprano-kenny':.01000000536441803,'alto-deep':0,'tenor-mellow':0,'alto-breath-growl':0}
  self.assertEqual(len(AUDIT['rows']),60);self.assertEqual(sum(r['swam'] for r in AUDIT['rows']),40)
  for row in AUDIT['rows']:
   with self.subTest(tone=row['id']),zipfile.ZipFile(R/'assets/tone-packages'/row['asset']) as z:
    self.assertIsNone(z.testzip());self.assertEqual(sha((R/'assets/tone-packages'/row['asset']).read_bytes()),row['resultSHA'])
    for f in json.loads(z.read('manifest.json'))['files']:self.assertEqual(sha(z.read(f['path'])),f['sha256'])
    if not row['swam']:
     self.assertEqual(row['sourceSHA'],row['resultSHA']);self.assertFalse(row['changes']);continue
    p=E.fromstring(z.read('tone.xml')).find('PRESET');outer=E.fromstring(codec.xml_bytes(codec.decode(p.findtext('INSTRUMENT_STATE'))));raw=codec.xml_bytes(codec.decode(outer.findtext('IComponent'))[176:]);native=E.fromstring(raw)
    self.assertEqual(sha(raw),row['resultXML']);self.assertEqual(sha(restore_native(raw,row['changes'])),row['sourceXML'])
    params={e.get('id'):e for e in native.iter('PARAM')};routes={e.get('parameterId'):e for e in native.iter('MIDIRemappingEntry')}
    self.assertEqual(float(params['mainVolume'].get('value')),0.0)
    expr=routes['expression'];self.assertEqual([expr.get(k) for k in ('enabled','messageType','msb','lsb')],['1','2','2','34'])
    volume=routes.get('mainVolume')
    if volume is not None and volume.get('enabled')=='1':self.assertNotIn(volume.get('msb'),('2','34','11'))
    for change in row['changes']:
     if 'parameter' in change:self.assertIn(change['parameter'],{'mainVolume'}|({'voiceComb1Wet'} if row['id'] in harmonics else set()))
     elif 'curve' in change:self.assertEqual(change['curve'],'formant');self.assertIn(row['id'],forms)
     else:
      before,after=change['before'],change['after'];delta={k for k in before if before[k]!=after[k]}
      if after['enabled']=='0':self.assertIn(change['route'],('mainVolume','voiceComb1Wet'));self.assertEqual(delta,{'enabled'})
      else:self.assertFalse(row['id'].startswith('dj-source-'));self.assertEqual((before['msb'],before['messageType']),('11','1'));self.assertEqual(delta,{'msb','lsb','messageType'})
    if row['id'] in harmonics:self.assertAlmostEqual(float(params['voiceComb1Wet'].get('value')),harmonics[row['id']]);self.assertEqual(routes['voiceComb1Wet'].get('enabled'),'0')
    if row['id'] in forms:
     curve=routes['formant'].find('MIDIRemappingCurve');lo,hi=forms[row['id']]
     self.assertEqual({k:float(curve.get(k)) for k in ('input_min','input_max','out_min','out_max','shape','symmetry','bypass','bipolar')},dict(input_min=0,input_max=127,out_min=lo,out_max=hi,shape=0,symmetry=.5,bypass=0,bipolar=0))
 def test_selection_order_and_current_media(self):
  text=(R/'native/Source/PublishedToneCatalog.h').read_text(encoding='utf-8');actual=re.findall(r'\{ "([^"]+)", "([^"]+)",',text)
  self.assertEqual(actual,[(r['id'],r['name']) for r in AUDIT['rows']]);ids=[r[0] for r in actual]
  removed={f'dj-source-{i:02d}' for i in (4,8,9,11,12,17,21,22,23,24,26,33,34)};self.assertFalse(removed&set(ids))
  i=ids.index('tenor-air-pocket');self.assertEqual(ids[i:i+3],['tenor-air-pocket','dj-source-16','dj-source-18'])
  app=(R/'prototype/app.js').read_text(encoding='utf-8');start=app.index('window.FENGYIN_TONE_MEDIA = ')+len('window.FENGYIN_TONE_MEDIA = ');media=json.JSONDecoder().raw_decode(app[start:])[0]
  self.assertEqual(list(media),ids)
  voices=json.loads((R/'prototype/voice-catalog.js').read_text(encoding='utf-8').split('=',1)[1].strip().rstrip(';'));voiceids={i['id'] for i in voices['items']}
  for ident,name in actual:
   self.assertEqual(media[ident]['name'],name);self.assertIn(media[ident]['voiceId'],voiceids);self.assertTrue((R/'assets/voice'/f"{media[ident]['voiceId']}.wav").exists())
if __name__=='__main__':unittest.main()
