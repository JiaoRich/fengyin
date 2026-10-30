"""Integrate only the approved complete Lingyuyan batch after byte/PCM checks."""
from pathlib import Path
import hashlib,json,re,shutil,wave
ROOT=Path(__file__).resolve().parents[1]
VOICE=ROOT/'assets/voice'
def main(source):
 source=Path(source);records=json.loads((source/'manifest.json').read_text(encoding='utf-8'))
 plan=json.loads((source.parent/'all-voice-plan.json').read_text(encoding='utf-8'))
 assert plan['voice']=='x6_lingyuyan_pro' and len(records)==len(plan['prompts'])==152
 assert {r['id']:r['text'] for r in records}==dict(plan['prompts'])
 catalog_path=ROOT/'prototype/voice-catalog.js'
 catalog=json.loads(catalog_path.read_text(encoding='utf-8').split('=',1)[1].strip().rstrip(';'))
 old={r['id']:r for r in catalog['items']};audit=[]
 for r in records:
  assert r['voice']==plan['voice'] and r['settings']['vcn']==plan['voice']
  raw=(source/(r['id']+'.wav')).read_bytes();assert hashlib.sha256(raw).hexdigest()==r['sha256']
  with wave.open(str(source/(r['id']+'.wav'))) as wav:
   assert (wav.getnchannels(),wav.getsampwidth(),wav.getframerate())==(1,2,24000)
   assert wav.getnframes()>2000 and any(wav.readframes(wav.getnframes()))
  item=old.get(r['id'],{'id':r['id'],'scene':'音色播报' if r['id'].startswith('TONE_140') else '操作提示','group':'风吟语音'})
  spoken=item.get('speechText',r['text']) if item.get('text')==r['text'] else r['text']
  item.update(text=r['text'],speechText=spoken,voice=plan['voice']);old[r['id']]=item
  audit.append({**r,'speechText':spoken,'status':'success'})
 # No voice remains silently backed by an old system or expired XFYUN clip.
 retained={r['id'] for r in records}|{'REC_BEGIN_BEEP'};retired=[]
 backup=source.parent/'retired-voice-backup';backup.mkdir(exist_ok=True)
 for p in VOICE.glob('*.wav'):
  if p.stem not in retained:
   retired.append(p.stem);shutil.copy2(p,backup/p.name);p.unlink()
 for r in records:shutil.copy2(source/(r['id']+'.wav'),VOICE/(r['id']+'.wav'))
 catalog.update(voice=plan['voice'],items=[old[r['id']] for r in records])
 catalog_path.write_text('window.FENGYIN_VOICE_CATALOG = '+json.dumps(catalog,ensure_ascii=False,separators=(',',':'))+';\n',encoding='utf-8')
 (VOICE/'release-140-audit.json').write_text(json.dumps(audit,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
 for name in ['recording-xfyun-audit.json','update-121-audit.json']:
  previous=json.loads((VOICE/name).read_text(encoding='utf-8'));ids={r['id'] for r in previous}
  (VOICE/name).write_text(json.dumps([r for r in audit if r['id'] in ids],ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
 (VOICE/'release-140-retired.json').write_text(json.dumps({'reason':'Unreferenced legacy speech; active prompts entirely regenerated with approved Lingyuyan','ids':sorted(retired)},ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
 pending=VOICE/'tone-140-pending.json'
 if pending.exists():pending.unlink()
 print('Integrated',len(records),'approved Lingyuyan speech clips; retired',len(retired),'unreferenced clips; retained existing beep.')
if __name__=='__main__':
 import sys
 main(sys.argv[1])
