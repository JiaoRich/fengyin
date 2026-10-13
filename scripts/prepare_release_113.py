"""Mechanical metadata rename; preserve all opaque tone state and parameters."""
from pathlib import Path
import json,zipfile,hashlib,subprocess,xml.etree.ElementTree as ET
ROOT=Path(__file__).resolve().parents[1]
path=ROOT/'assets/tone-packages/soprano-kenny.fytonepack'
with zipfile.ZipFile(path) as archive: files={name:archive.read(name) for name in archive.namelist()}
old=files['tone.xml'];root=ET.fromstring(old);preset=root.find('PRESET')
assert preset is not None and preset.get('name') in ('高萨-肯尼基','高萨-原声')
preset.set('name','高萨-原声');new=ET.tostring(root,encoding='utf-8',xml_declaration=True)
manifest=json.loads(files['manifest.json']);manifest['style']='高萨-原声'
for item in manifest['files']:
    if item['path']=='tone.xml': item.update(bytes=len(new),sha256=hashlib.sha256(new).hexdigest())
files['tone.xml']=new;files['manifest.json']=json.dumps(manifest,ensure_ascii=False,indent=2).encode('utf-8')
temporary=path.with_suffix('.renamed')
with zipfile.ZipFile(temporary,'w',compression=zipfile.ZIP_DEFLATED) as archive:
    for name,data in files.items():
        info=zipfile.ZipInfo(name,(2026,10,13,0,0,0));info.compress_type=zipfile.ZIP_DEFLATED
        archive.writestr(info,data)
temporary.replace(path)
style=ROOT/'native/Source/ToneStyleCatalog.h';text=style.read_text(encoding='utf-8')
assert text.count('s.name=juce::String::fromUTF8("肯尼基")')+text.count('s.name=juce::String::fromUTF8("原声")')==1
style.write_text(text.replace('s.name=juce::String::fromUTF8("肯尼基")','s.name=juce::String::fromUTF8("原声")'),encoding='utf-8')
catalogPath=ROOT/'prototype/voice-catalog.js'
catalog=json.loads(catalogPath.read_text(encoding='utf-8').split('=',1)[1].strip().rstrip(';'))
assert catalog['voice']=='Tingting'
item=next(i for i in catalog['items'] if i['id']=='V014_01')
item.update(text='高音萨克斯，原声。',speechText='高音萨克斯，原声。')
temporary=Path('/private/tmp/fengyin-113-rename.aiff')
subprocess.run(['/usr/bin/say','-v','Tingting','-r','190','-o',str(temporary),item['speechText']],check=True)
target=ROOT/'assets/voice/V014_01.wav'
subprocess.run(['/usr/bin/afconvert','-f','WAVE','-d','LEI16@24000','-c','1',str(temporary),str(target)],check=True)
assert target.stat().st_size>4096
catalogPath.write_text('window.FENGYIN_VOICE_CATALOG='+json.dumps(catalog,ensure_ascii=False)+';\n',encoding='utf-8')
html=ROOT/'prototype/index.html'
html.write_text(html.read_text(encoding='utf-8').replace('风吟 1.1.12','风吟 1.1.13'),encoding='utf-8')
