"""Rename two released tones in the same reviewed offline voice."""
import json,subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[1]
path=root/'prototype/voice-catalog.js'
catalog=json.loads(path.read_text(encoding='utf-8').split('=',1)[1].strip().rstrip(';'))
changes={'V014_20':'小提琴。','V014_22':'大提琴。'}
for item in catalog['items']:
    if item['id'] not in changes:continue
    text=changes[item['id']];item.update(text=text,speechText=text,voice=catalog['voice'])
    temporary=Path('/private/tmp')/f"fengyin-112-{item['id']}.aiff"
    subprocess.run(['/usr/bin/say','-v',catalog['voice'],'-r','190','-o',str(temporary),text],check=True)
    target=root/'assets/voice'/f"{item['id']}.wav"
    subprocess.run(['/usr/bin/afconvert','-f','WAVE','-d','LEI16@24000','-c','1',str(temporary),str(target)],check=True)
    assert target.stat().st_size>4096
path.write_text('window.FENGYIN_VOICE_CATALOG='+json.dumps(catalog,ensure_ascii=False)+';\n',encoding='utf-8')
