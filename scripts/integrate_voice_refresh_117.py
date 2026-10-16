"""Integrate the three explicitly requested recordings; never submit TTS requests."""
import hashlib
import json
import shutil
import wave
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]

def main():
    source=ROOT/'outputs/voice-117-renames';voice=ROOT/'assets/voice'
    records=json.loads((source/'manifest.json').read_text(encoding='utf-8'))
    assert len(records)==4 and all(x['status']=='success' for x in records)
    catalog_file=ROOT/'prototype/voice-catalog.js'
    catalog=json.loads(catalog_file.read_text(encoding='utf-8').split('=',1)[1].strip().rstrip(';'))
    base=voice/'release-117-audit.json'
    audit=json.loads((base if base.exists() else voice/'release-116-audit.json').read_text(encoding='utf-8'))
    backup=ROOT/'outputs/tone-refresh-117-before';backup.mkdir(parents=True,exist_ok=True)
    for record in records:
        ident=record['id'];old_ident='TONE_VIOLIN_SWEET' if ident=='TONE_VIOLIN_BRIGHT' else ident
        clip=source/(ident+'.wav');assert hashlib.sha256(clip.read_bytes()).hexdigest()==record['sha256']
        with wave.open(str(clip)) as w:
            assert w.getframerate()==24000 and w.getnchannels()==1 and w.getsampwidth()==2
            assert w.getnframes()>2400 and any(w.readframes(w.getnframes()))
        item=next(x for x in catalog['items'] if x['id'] in (old_ident,ident))
        item.update({k:record[k] for k in ['id','text','speechText','voice']})
        row=next(x for x in audit if x['id'] in (old_ident,ident))
        row.update({k:record[k] for k in ['id','text','speechText','voice','sha256']})
        previous=voice/(old_ident+'.wav')
        if previous.exists() and not (backup/previous.name).exists():shutil.copy2(previous,backup/previous.name)
        temporary=voice/(ident+'.wav.refresh-tmp');shutil.copy2(clip,temporary);temporary.replace(voice/(ident+'.wav'))
    old=voice/'TONE_VIOLIN_SWEET.wav'
    if old.exists():old.unlink()  # Retired prompt has a recoverable backup above.
    assert len(catalog['items'])==123 and len({x['id'] for x in catalog['items']})==123
    catalog_file.write_text('window.FENGYIN_VOICE_CATALOG = '+json.dumps(catalog,ensure_ascii=False,separators=(',',':'))+';\n',encoding='utf-8')
    base.write_text(json.dumps(audit,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    print('Integrated three midnight-radio recordings; retired passion prompt; 123 clips remain.')

if __name__=='__main__':main()
