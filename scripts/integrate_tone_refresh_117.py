"""User-approved tone replacements; preserve opaque states and all unrelated settings."""
import hashlib
import json
import re
import shutil
import zipfile
from pathlib import Path
import xml.etree.ElementTree as ET

ROOT=Path(__file__).resolve().parents[1]
ASSETS=ROOT/'assets/tone-packages'
INCOMING=ROOT/'assets/incoming-tones/20261017'
BACKUP=ROOT/'outputs/tone-refresh-117-before'

def sha(data):return hashlib.sha256(data).hexdigest()

def rename_package(source,destination,name,attack=False):
    with zipfile.ZipFile(source) as z:
        infos=z.infolist();payload={i.filename:z.read(i) for i in infos}
        assert z.testzip() is None
    old=payload['tone.xml'];preset=ET.fromstring(old).find('PRESET');before_name=preset.get('name')
    before_preset=ET.fromstring(old).find('PRESET')
    changed=old
    original_name_attribute=re.search(rb'<PRESET\b[^>]*?\bname="([^"]*)"',old).group(1)
    if before_name!=name:
        target=b'name="'+original_name_attribute+b'"'
        assert changed.count(target)==1
        changed=changed.replace(target,('name="'+name+'"').encode())
    if attack:
        target=b'id="index:87:attackcontrol" value="0.5"'
        assert changed.count(target)==1
        changed=changed.replace(target,b'id="index:87:attackcontrol" value="0.6666666865348816"')
    after_preset=ET.fromstring(changed).find('PRESET')
    for tag in ['INSTRUMENT_STATE','SAMPLER_STATE','EFFECT_CHAIN','CONTAINER_PROJECT_STATE']:
        a,b=before_preset.find(tag),after_preset.find(tag)
        assert (ET.tostring(a) if a is not None else None)==(ET.tostring(b) if b is not None else None)
    manifest=json.loads(payload['manifest.json']);manifest['style']=name
    for entry in manifest['files']:
        assert sha(payload[entry['path']])==entry['sha256']
    payload['tone.xml']=changed
    for entry in manifest['files']:
        data=payload[entry['path']];entry.update(bytes=len(data),sha256=sha(data))
    payload['manifest.json']=(json.dumps(manifest,ensure_ascii=False,indent=2)+'\n').encode()
    with zipfile.ZipFile(destination,'w') as z:
        for info in infos:z.writestr(info,payload[info.filename])
    return dict(asset=destination.name,name=name,beforeName=before_name,beforeNameAttribute=original_name_attribute.decode('utf-8'),sourceSha256=sha(source.read_bytes()),
                afterSha256=sha(destination.read_bytes()),attackSnapshotCorrected=attack,
                opaqueStatesUnchanged=True,outputGain=after_preset.get('outputGain'),dependencies=manifest['dependencies'])

def main():
    audit_path=ASSETS/'tone-refresh-117-audit.json'
    if audit_path.exists():
        audit=json.loads(audit_path.read_text(encoding='utf-8'))
        assert all(sha((ASSETS/x['asset']).read_bytes())==x['afterSha256'] for x in audit)
        print('Tone refresh already applied and verified.');return
    INCOMING.mkdir(parents=True,exist_ok=True);BACKUP.mkdir(parents=True,exist_ok=True)
    sources=[Path('/Users/mengyang.jmy/Library/Containers/com.tencent.xinWeChat/Data/Documents/xwechat_files/wxid_352d1hnvv8kq22_d099/msg/file/2026-10/高音萨克斯-高萨-原声.fytonepack'),
             Path('/Users/mengyang.jmy/Desktop/小提琴-明亮.fytonepack')]
    for source in sources:
        target=INCOMING/source.name
        if target.exists():assert sha(target.read_bytes())==sha(source.read_bytes())
        else:shutil.copy2(source,target)
    for name in ['soprano-kenny.fytonepack','violin-sweet.fytonepack','violin-soft.fytonepack','refresh-114-audit.json','output-gain-117-audit.json']:
        if not (BACKUP/name).exists():shutil.copy2(ASSETS/name,BACKUP/name)
    # Soprano is a byte-for-byte user package replacement, not a parameter rewrite.
    temporary=ASSETS/'soprano-kenny.fytonepack.refresh-tmp'
    shutil.copy2(sources[0],temporary);temporary.replace(ASSETS/'soprano-kenny.fytonepack')
    with zipfile.ZipFile(sources[0]) as z:
        manifest=json.loads(z.read('manifest.json'));preset=ET.fromstring(z.read('tone.xml')).find('PRESET')
    soprano=dict(asset='soprano-kenny.fytonepack',name='高萨-原声',beforeName=preset.get('name'),
        sourceSha256=sha(sources[0].read_bytes()),afterSha256=sha(sources[0].read_bytes()),
        attackSnapshotCorrected=False,opaqueStatesUnchanged=True,outputGain=preset.get('outputGain'),dependencies=manifest['dependencies'])
    bright=rename_package(INCOMING/sources[1].name,ASSETS/'violin-bright.fytonepack','小提琴-明亮',True)
    soft=rename_package(BACKUP/'violin-soft.fytonepack',ASSETS/'violin-soft.fytonepack','小提琴-柔美')
    audit_path.write_text(json.dumps([soprano,bright,soft],ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    gain_path=ASSETS/'output-gain-117-audit.json';gain=json.loads(gain_path.read_text(encoding='utf-8'))
    row=next(x for x in gain['targets'] if x['file']=='violin-soft.fytonepack')
    row['preRenamePackageSha256']=row['afterPackageSha256'];row['afterPackageSha256']=soft['afterSha256']
    row['displayRename']={'before':'柔美小提琴','after':'小提琴-柔美'}
    gain_path.write_text(json.dumps(gain,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    historical_path=ASSETS/'refresh-114-audit.json';historical=json.loads(historical_path.read_text(encoding='utf-8'))
    for entry in historical:
        if entry['asset']=='violin-sweet.fytonepack':
            entry.update(asset=bright['asset'],name=bright['name'],sourceSha256=bright['sourceSha256'],dependencies=bright['dependencies'],
                         approvedParameterChanges=['Attack snapshot 0.5 -> Expression 0.6666666865348816; user approved; full attackType=2 already Expression'],completeStateUnchanged=True)
        if entry['asset']=='violin-soft.fytonepack':entry['name']='小提琴-柔美'
    historical_path.write_text(json.dumps(historical,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    (ASSETS/'violin-sweet.fytonepack').unlink()  # User requested retirement; recoverable backup above.
    print('Replaced soprano; retired passion violin; added bright; renamed soft; original states preserved.')

if __name__=='__main__':main()
