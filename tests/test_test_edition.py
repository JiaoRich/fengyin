"""Approved migration protects every non-reverb byte-level state and setting."""
import copy,hashlib,json,re,unittest,zipfile,wave
from pathlib import Path
import xml.etree.ElementTree as E
R=Path(__file__).resolve().parents[1]
sha=lambda b:hashlib.sha256(b).hexdigest()
class TestEdition(unittest.TestCase):
 def test_all_73_preserve_non_reverb_content(self):
  rows=json.loads((R/'assets/tone-packages/test-edition-audit.json').read_text(encoding='utf-8'))
  self.assertEqual(len(rows),73)
  for row in rows:
   with self.subTest(tone=row['id']):
    raw=(R/'assets/tone-packages'/row['asset']).read_bytes();self.assertEqual(sha(raw),row['sha256'])
    with zipfile.ZipFile(R/'assets/tone-packages'/row['asset']) as z:
     self.assertIsNone(z.testzip());p=E.fromstring(z.read('tone.xml')).find('PRESET')
     m=json.loads(z.read('manifest.json'));self.assertEqual(m['version'],4)
     for item in m['files']:self.assertEqual(sha(z.read(item['path'])),item['sha256'])
    self.assertEqual(p.get('performanceReverb'),'1');self.assertEqual(float(p.get('reverbMix')), .2)
    self.assertEqual(p.get('name'),row['name'] if row['id'].startswith('dj-source-') else row['previousName'])
    last=p.findall('SERIAL_EFFECT')[-1];self.assertEqual(last.get('bypassed'),'0')
    self.assertEqual(E.fromstring(last.findtext('DESCRIPTION')).get('name'),'Pro-R 2')
    p.remove(last);p.attrib.pop('performanceReverb');p.set('reverbMix',row['previousReverbMix']);p.set('name',row['previousName'])
    self.assertEqual(sha(E.tostring(p)),row['protectedContentSHA'])
 def test_names_voice_and_identity(self):
  catalog=(R/'native/Source/PublishedToneCatalog.h').read_text(encoding='utf-8')
  names=re.findall(r'\{ "(dj-source-\d+)", "([^"]+)"',catalog)
  self.assertEqual(len(names),29);app=(R/'prototype/app.js').read_text(encoding='utf-8')
  v=json.loads((R/'prototype/voice-catalog.js').read_text(encoding='utf-8').split('=',1)[1].strip().rstrip(';'))
  voices={i['id']:i for i in v['items']}
  for ident,name in names:
   self.assertTrue(name.startswith('#'));voice='TONE_TEST_'+ident[-2:]
   self.assertEqual(voices[voice]['speechText'],name[1:]+'。')
   self.assertEqual(voices[voice]['voice'],'x6_lingyuyan_pro')
   self.assertRegex(app,re.escape('"'+ident+'"')+r':\s*\{[^}]*'+re.escape('"'+voice+'"'))
   with wave.open(str(R/'assets/voice'/f'{voice}.wav')) as w:
    self.assertEqual(w.getframerate(),24000);self.assertEqual(w.getnchannels(),1);self.assertGreater(w.getnframes(),12000)
if __name__=='__main__':unittest.main()
