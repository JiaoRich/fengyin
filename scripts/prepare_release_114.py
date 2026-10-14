"""User-approved package metadata refresh. Preserve all complete plugin states."""
from pathlib import Path
import json,zipfile,hashlib,xml.etree.ElementTree as ET,shutil,subprocess
ROOT=Path(__file__).resolve().parents[1]
SOURCE=Path('/Users/mengyang.jmy/Library/Containers/com.tencent.xinWeChat/Data/Documents/xwechat_files/wxid_352d1hnvv8kq22_d099/msg/file/2026-10')
items=[('甜美小提琴','violin-sweet'),('柔美小提琴','violin-soft'),('双簧管','oboe'),('长笛','flute'),('短笛','piccolo'),('单簧管','clarinet')]
audit=[]
for name,asset in items:
    source=SOURCE/name
    with zipfile.ZipFile(source) as z:
        assert z.testzip() is None
        files={n:z.read(n) for n in z.namelist()}
    m=json.loads(files['manifest.json']);root=ET.fromstring(files['tone.xml']);p=root.find('PRESET');assert p is not None
    for f in m['files']:
        assert len(files[f['path']])==f['bytes'] and hashlib.sha256(files[f['path']]).hexdigest()==f['sha256']
    originalState=p.find('INSTRUMENT_STATE').text
    p.set('name',name);p.set('instrumentChineseName','小提琴' if asset.startswith('violin') else name)
    p.set('studioDraft','0');p.set('published','0')
    changes=[]
    if asset=='violin-sweet':
        for param in p.iter('TONE_PARAMETER'):
            if param.get('id')=='index:87:attackcontrol':
                assert float(param.get('value'))==.5
                param.set('value','0.6666666865348816');changes.append('Attack snapshot -> Expression; original attackType=2 state preserved, user authorised')
        assert changes
    assert p.find('INSTRUMENT_STATE').text==originalState
    data=ET.tostring(root,encoding='utf-8',xml_declaration=True);m['style']=name;m['instrument']=p.get('instrumentChineseName')
    for f in m['files']:
        if f['path']=='tone.xml':f.update(bytes=len(data),sha256=hashlib.sha256(data).hexdigest())
    target=ROOT/'assets/tone-packages'/f'{asset}.fytonepack'
    with zipfile.ZipFile(target,'w',zipfile.ZIP_DEFLATED) as z:
        z.writestr('tone.xml',data);z.writestr('manifest.json',json.dumps(m,ensure_ascii=False,indent=2))
    audit.append({'name':name,'sourceSha256':hashlib.sha256(source.read_bytes()).hexdigest(),'asset':target.name,'dependencies':m['dependencies'],'approvedParameterChanges':changes,'completeStateUnchanged':True})
(ROOT/'assets/tone-packages/refresh-114-audit.json').write_text(json.dumps(audit,ensure_ascii=False,indent=2))
magic=Path('/Users/mengyang.jmy/Library/Containers/com.tencent.xinWeChat/Data/Documents/xwechat_files/wxid_352d1hnvv8kq22_d099/temp/RWTemp/2026-10/f1d4d33e2e6dab58e24dd56a246af791/magic.CURVE-1.0.2-Windows-Setup.exe')
dest=ROOT/'assets/components'/magic.name;dest.parent.mkdir(exist_ok=True);shutil.copyfile(magic,dest)
assert hashlib.sha256(dest.read_bytes()).digest()==hashlib.sha256(magic.read_bytes()).digest()
catalogPath=ROOT/'prototype/voice-catalog.js';catalog=json.loads(catalogPath.read_text().split('=',1)[1].strip().rstrip(';'))
new=[('MT_SLOW','慢速乐曲。'),('MT_MEDIUM','中速乐曲。'),('MT_FAST','快速乐曲。'),('MT_GUIDE','选择乐曲速度，可以调整自动颤音和混响预延迟。'),('TONE_VIOLIN_SWEET','甜美小提琴。'),('TONE_VIOLIN_SOFT','柔美小提琴。')]
for id,text in new:
    item={'id':id,'text':text,'speechText':text,'scene':'演奏设置','group':'乐曲速度与新音色','voice':'Tingting'}
    catalog['items']=[x for x in catalog['items'] if x['id']!=id];catalog['items'].append(item)
    aiff=Path('/private/tmp')/(id+'.aiff')
    subprocess.run(['/usr/bin/say','-v','Tingting','-r','190','-o',str(aiff),text],check=True)
    wav=ROOT/'assets/voice'/(id+'.wav')
    subprocess.run(['/usr/bin/afconvert','-f','WAVE','-d','LEI16@24000','-c','1',str(aiff),str(wav)],check=True)
    assert wav.stat().st_size>4096
catalogPath.write_text('window.FENGYIN_VOICE_CATALOG='+json.dumps(catalog,ensure_ascii=False)+';\n')
for relative in ['scripts/build-windows.ps1','installer/FengYin.iss','prototype/index.html']:
    path=ROOT/relative;path.write_text(path.read_text(encoding='utf-8-sig').replace('1.1.13','1.1.14'),encoding='utf-8')
print('Six complete tone states preserved; approved Attack snapshot fixed; dependency and speech assets prepared')
