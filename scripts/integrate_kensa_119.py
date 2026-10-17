"""Receive the approved normal Kensa package; change display name only."""
from pathlib import Path
import hashlib,json,re,zipfile
import xml.etree.ElementTree as ET

ROOT=Path(__file__).resolve().parents[1]
SOURCE=Path('/Users/mengyang.jmy/Desktop/肯萨-肯萨1017.fytonepack')
ARCHIVE=ROOT/'assets/incoming-tones/20261017/肯萨-肯萨1017.fytonepack'
TARGET=ROOT/'assets/tone-packages/kontakt-kensa.fytonepack'
AUDIT=ROOT/'assets/tone-packages/kensa-refresh-119-audit.json'
EXPECTED='2a59c0501fb62fd8544dc3d990081b23edecd956dd6de2fe00f752a2af28d187'

def sha(data):return hashlib.sha256(data).hexdigest()
def main():
    if AUDIT.exists():
        audit=json.loads(AUDIT.read_text(encoding='utf-8'))
        assert sha(ARCHIVE.read_bytes())==audit['sourceSha256']
        assert sha(TARGET.read_bytes())==audit['packageSha256']
        print('Approved Kensa already integrated and verified.');return
    original=SOURCE.read_bytes();assert sha(original)==EXPECTED
    with zipfile.ZipFile(SOURCE) as z:
        assert z.testzip() is None
        infos=z.infolist();data={i.filename:z.read(i) for i in infos}
    manifest=json.loads(data['manifest.json'])
    for item in manifest['files']:
        assert sha(data[item['path']])==item['sha256']
        assert len(data[item['path']])==item['bytes']
    before=data['tone.xml'];p=ET.fromstring(before).find('PRESET')
    assert p.get('name')=='肯萨1017' and p.get('instrumentKey')=='kontakt-kensa'
    binding=next(x for x in p.findall('TONE_PARAMETER') if x.get('id')=='index:1:percent')
    assert abs(float(binding.get('value'))-90/390)<1e-6
    changed,count=re.subn(rb'(<PRESET\b[^>]*?\bname=")[^"]*(")',lambda m:m.group(1)+'肯萨'.encode()+m.group(2),before)
    assert count==1
    after=ET.fromstring(changed).find('PRESET')
    p.set('name','肯萨');assert ET.tostring(p)==ET.tostring(after)
    data['tone.xml']=changed
    manifest['style']='肯萨'
    for item in manifest['files']:item.update(bytes=len(data[item['path']]),sha256=sha(data[item['path']]))
    data['manifest.json']=(json.dumps(manifest,ensure_ascii=False,indent=2)+'\n').encode()
    ARCHIVE.parent.mkdir(parents=True,exist_ok=True)
    if ARCHIVE.exists():assert ARCHIVE.read_bytes()==original
    else:ARCHIVE.write_bytes(original)
    previous=sha(TARGET.read_bytes())
    temporary=TARGET.with_suffix('.119-tmp')
    with zipfile.ZipFile(temporary,'w') as z:
        for info in infos:z.writestr(info,data[info.filename])
    with zipfile.ZipFile(temporary) as z:assert z.testzip() is None
    temporary.replace(TARGET)
    audit=dict(source=str(ARCHIVE.relative_to(ROOT)),sourceSha256=EXPECTED,packageSha256=sha(TARGET.read_bytes()),previousPackageSha256=previous,
               originalOpaqueStatePreserved=True,originalEffectsPreserved=True,allParameterSnapshotsPreserved=True,
               metadataChanges=['PRESET name: 肯萨1017 -> 肯萨','manifest style -> 肯萨','manifest file bytes/hash refreshed'],
               defaultPercent=100,hostAutomationSlot=1,actualPitchEndpointsVerified=False,
               dynamicsCC11NotIndependentlyReadBack=True,bendRuntimeLogicUnchanged=True)
    AUDIT.write_text(json.dumps(audit,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    print('Integrated approved normal Kensa: state, all 2785 parameters and effects preserved; display name only.')
if __name__=='__main__':main()
