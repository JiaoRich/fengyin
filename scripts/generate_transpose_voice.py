"""Generate the transpose clips with the same Xiaoxiao voice as the main catalog."""
import asyncio
import json
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
NUMBERS = ['零','一','二','三','四','五','六','七','八','九','十','十一','十二']

async def main():
    path = ROOT / 'prototype/voice-catalog.js'
    catalog = json.loads(path.read_text(encoding="utf-8").split('=', 1)[1].strip().rstrip(';'))
    entries = [('T000', '关闭移调'), ('T001', '移调零表示跟随电吹管。加减按钮每次改变一个半音。')]
    entries += [(f'{prefix}{n:02d}', f'{word}{NUMBERS[n]}个半音') for prefix, word in [('TU','加'),('TD','减')] for n in range(1,13)]
    ids = {i for i, _ in entries}
    catalog['items'] = [i for i in catalog['items'] if i['id'] not in ids]
    catalog['items'] += [dict(id=i,text=t,speechText=t,scene='半音移调',group='移调引导') for i,t in entries]
    offline = '--uniform-offline' in sys.argv
    if offline:
        catalog['voice'] = 'Tingting'
        entries = [(i['id'], i.get('speechText', i['text'])) for i in catalog['items']]
    for ident, text in entries:
        temp = Path('/private/tmp') / f'fengyin-110-{ident}.aiff'
        if offline:
            subprocess.run(['/usr/bin/say','-v','Tingting','-r','190','-o',str(temp),text],check=True)
        else:
            import edge_tts
            temp = temp.with_suffix('.mp3')
            await edge_tts.Communicate(text, catalog['voice'], rate=catalog['rate'], pitch=catalog['pitch']).save(str(temp))
        subprocess.run(['/usr/bin/afconvert','-f','WAVE','-d','LEI16@24000','-c','1',str(temp),str(ROOT/'assets/voice'/f'{ident}.wav')], check=True)
        print(ident, flush=True)
    for item in catalog['items']:
        item['voice'] = catalog['voice']
    path.write_text('window.FENGYIN_VOICE_CATALOG=' + json.dumps(catalog,ensure_ascii=False) + ';\n', encoding='utf-8')

asyncio.run(main())
