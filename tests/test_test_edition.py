"""Approved migration protects every non-reverb byte-level state and setting."""
import copy,hashlib,json,re,unittest,zipfile,wave
from pathlib import Path
import xml.etree.ElementTree as E
R=Path(__file__).resolve().parents[1]
sha=lambda b:hashlib.sha256(b).hexdigest()
from test_release_147 import restore_state
CURRENT={r['id']:r for r in json.loads((R/'assets/tone-packages/release-147-audit.json').read_text(encoding='utf-8'))['rows']}
class TestEdition(unittest.TestCase):
 def test_all_73_preserve_non_reverb_content(self):
  rows=json.loads((R/'assets/tone-packages/test-edition-audit.json').read_text(encoding='utf-8'))
  self.assertEqual(len(rows),73)
  for row in rows:
   with self.subTest(tone=row['id']):
    raw=(R/'assets/tone-packages'/row['asset']).read_bytes();self.assertEqual(sha(raw),CURRENT.get(row['id'],{}).get('resultSHA',row['sha256']))
    with zipfile.ZipFile(R/'assets/tone-packages'/row['asset']) as z:
     self.assertIsNone(z.testzip());p=E.fromstring(z.read('tone.xml')).find('PRESET')
     m=json.loads(z.read('manifest.json'));self.assertEqual(m['version'],4)
     for item in m['files']:self.assertEqual(sha(z.read(item['path'])),item['sha256'])
    self.assertEqual(p.get('performanceReverb'),'1');self.assertEqual(float(p.get('reverbMix')), .2)
    newnames={'dj-source-27','cello','dj-source-28','dj-source-35','tenor-air-pocket'}
    self.assertEqual(p.get('name'),CURRENT[row['id']]['name'] if row['id'] in newnames else (row['name'] if row['id'].startswith('dj-source-') else row['previousName']))
    if row['id'] in CURRENT and CURRENT[row['id']]['swam']:
     e=p.find('INSTRUMENT_STATE');e.text=restore_state(e.text,CURRENT[row['id']]['changes'])
    last=p.findall('SERIAL_EFFECT')[-1];self.assertEqual(last.get('bypassed'),'0')
    self.assertEqual(E.fromstring(last.findtext('DESCRIPTION')).get('name'),'Pro-R 2')
    self.assertEqual(E.fromstring(last.findtext('DESCRIPTION')).get('version'),'2.0.6.0')
    self.assertEqual(m['dependencies'][-1]['version'],'2.0.6.0')
    p.remove(last);p.attrib.pop('performanceReverb');p.set('reverbMix',row['previousReverbMix']);p.set('name',row['previousName'])
    self.assertEqual(sha(E.tostring(p)),row['protectedContentSHA'])
 def test_names_voice_and_identity(self):
  catalog=(R/'native/Source/PublishedToneCatalog.h').read_text(encoding='utf-8')
  names=re.findall(r'\{ "(dj-source-\d+)", "([^"]+)"',catalog)
  self.assertEqual(len(names),16);app=(R/'prototype/app.js').read_text(encoding='utf-8')
  v=json.loads((R/'prototype/voice-catalog.js').read_text(encoding='utf-8').split('=',1)[1].strip().rstrip(';'))
  voices={i['id']:i for i in v['items']}
  for ident,name in names:
   self.assertTrue(name.startswith('#'));voice={'dj-source-27':'TONE_147_VIOLIN3','dj-source-28':'TONE_147_CELLO2','dj-source-35':'TONE_147_MATOUQIN'}.get(ident,'TONE_TEST_'+ident[-2:])
   spoken={'#小提琴3':'小提琴三。','#大提琴2':'大提琴二。'}.get(name,name[1:]+'。')
   self.assertEqual(voices[voice]['speechText'],spoken)
   self.assertEqual(voices[voice]['voice'],'x6_lingyuyan_pro')
   self.assertRegex(app,re.escape('"'+ident+'"')+r':\s*\{[^}]*'+re.escape('"'+voice+'"'))
   with wave.open(str(R/'assets/voice'/f'{voice}.wav')) as w:
    self.assertEqual(w.getframerate(),24000);self.assertEqual(w.getnchannels(),1);self.assertGreater(w.getnframes(),12000)
if __name__=='__main__':unittest.main()
