"""Approved catalogue metadata + speech integration, never edit native state."""
import hashlib,json,shutil,zipfile,xml.etree.ElementTree as ET
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
source=ROOT/'assets/incoming-tones/20261016/Kontakt音源-肯萨.fytonepack'
with zipfile.ZipFile(source) as z:
    assert z.testzip() is None
    data={n:z.read(n) for n in z.namelist()}
tree=ET.fromstring(data['tone.xml']);p=tree.find('PRESET')
before={n:ET.tostring(p.find(n)) for n in ['INSTRUMENT_STATE','INSTRUMENT_DESCRIPTION']}
p.set('instrumentKey','kontakt-kensa');p.set('instrumentChineseName','肯萨')
data['tone.xml']=ET.tostring(tree,encoding='utf-8',xml_declaration=True)
manifest=json.loads(data['manifest.json'])
for f in manifest['files']:
    f['bytes']=len(data[f['path']]);f['sha256']=hashlib.sha256(data[f['path']]).hexdigest()
data['manifest.json']=json.dumps(manifest,ensure_ascii=False).encode()
target=ROOT/'assets/tone-packages/kontakt-kensa.fytonepack'
with zipfile.ZipFile(target,'w',zipfile.ZIP_DEFLATED) as z:
    for name,content in data.items():z.writestr(name,content)
assert before=={n:ET.tostring(ET.fromstring(data['tone.xml']).find('PRESET').find(n)) for n in before}
catalogpath=ROOT/'prototype/voice-catalog.js'
catalog=json.loads(catalogpath.read_text().split('=',1)[1].strip().rstrip(';'))
if not any(x['id']=='TONE_KENSA' for x in catalog['items']):
    catalog['items'].append(dict(id='TONE_KENSA',text='肯萨。',speechText='肯萨',voice='x6_wuyediantai_mini',scene='音色',group='音色播报'))
catalogpath.write_text('window.FENGYIN_VOICE_CATALOG = '+json.dumps(catalog,ensure_ascii=False,separators=(',',':'))+';\n')
shutil.copy2(ROOT/'outputs/voice-116-replacements/TONE_KENSA.wav',ROOT/'assets/voice/TONE_KENSA.wav')
audit=[]
for x in catalog['items']:
    audio=ROOT/'assets/voice'/(x['id']+'.wav')
    audit.append(dict(id=x['id'],text=x['text'],speechText=x.get('speechText',x['text']),voice=x['voice'],sha256=hashlib.sha256(audio.read_bytes()).hexdigest()))
(ROOT/'assets/voice/release-116-audit.json').write_text(json.dumps(audit,ensure_ascii=False,indent=2)+'\n')
print('Kensa catalogue package + voice integrated; full plugin state unchanged.')
