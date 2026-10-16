"""Install the approved Soprano package byte-for-byte; retain a backup."""
import hashlib
import json
import shutil
import zipfile
from pathlib import Path
import xml.etree.ElementTree as ET

ROOT=Path(__file__).resolve().parents[1]
SOURCE=Path('/Users/mengyang.jmy/Library/Containers/com.tencent.xinWeChat/Data/Documents/xwechat_files/wxid_352d1hnvv8kq22_d099/temp/RWTemp/2026-10/9e20f478899dc29eb19741386f9343c8/高音萨克斯-高萨-明亮.fytonepack')
def main():
    data=SOURCE.read_bytes()
    with zipfile.ZipFile(SOURCE) as z:
        assert z.testzip() is None
        manifest=json.loads(z.read('manifest.json'))
        for entry in manifest['files']:
            body=z.read(entry['path'])
            assert len(body)==entry['bytes'] and hashlib.sha256(body).hexdigest()==entry['sha256']
        p=ET.fromstring(z.read('tone.xml')).find('PRESET')
        assert p.get('name')=='高萨-明亮' and p.get('instrumentKey')=='soprano-sax'
        assert next(x for x in p.iter('TONE_PARAMETER') if x.get('id','').endswith(':attackcontrol')).get('value')=='1.0'
    incoming=ROOT/'assets/incoming-tones/20261017'/SOURCE.name
    incoming.parent.mkdir(parents=True,exist_ok=True)
    if incoming.exists():assert incoming.read_bytes()==data
    else:shutil.copy2(SOURCE,incoming)
    target=ROOT/'assets/tone-packages/soprano-pop.fytonepack'
    backup=ROOT/'outputs/tone-refresh-117-before/soprano-pop.fytonepack'
    if target.exists() and not backup.exists():
        backup.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(target,backup)
    shutil.copy2(incoming,target)
    audit={'asset':target.name,'name':'高萨-明亮','sha256':hashlib.sha256(data).hexdigest(),
           'byteExact':True,'attackSnapshot':1,'fullAttackType':2,'attackMeaning':'Expression'}
    (ROOT/'assets/tone-packages/soprano-bright-117-audit.json').write_text(json.dumps(audit,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    print('Verified and installed byte-exact Soprano Bright package')
if __name__=='__main__':main()
