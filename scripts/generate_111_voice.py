"""Update only revised guide clips, preserving the unified offline voice."""
import json
import subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[1]
path=root/'prototype/voice-catalog.js'
catalog=json.loads(path.read_text(encoding='utf-8').split('=',1)[1].strip().rstrip(';'))
changes={'T001':'请先将电吹管设置为C调，再在演奏调中选择歌曲需要的调。',
         'V047':'请轻咬吹嘴，让风吟识别咬嘴控制信号。弯音信号不会用于技巧映射。'}
for item in catalog['items']:
    if item['id'] not in changes:continue
    text=changes[item['id']];item['text']=text;item['speechText']=text;item['voice']=catalog['voice']
    temp=Path('/private/tmp')/f"fengyin-111-{item['id']}.aiff"
    subprocess.run(['/usr/bin/say','-v',catalog['voice'],'-r','190','-o',str(temp),text],check=True)
    target=root/'assets/voice'/f"{item['id']}.wav"
    subprocess.run(['/usr/bin/afconvert','-f','WAVE','-d','LEI16@24000','-c','1',str(temp),str(target)],check=True)
    assert target.stat().st_size>4096
path.write_text('window.FENGYIN_VOICE_CATALOG='+json.dumps(catalog,ensure_ascii=False)+';\n',encoding='utf-8')
