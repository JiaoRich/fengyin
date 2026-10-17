"""Receive the user-exported Kensa Host Automation binding; identity-only rewrite."""
from pathlib import Path
import hashlib
import json
import re
import zipfile
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]
SOURCE = Path('/Users/mengyang.jmy/Library/Containers/com.tencent.xinWeChat/Data/Documents/xwechat_files/wxid_352d1hnvv8kq22_d099/temp/RWTemp/2026-10/9e20f478899dc29eb19741386f9343c8/Kontakt音源-肯萨(1).fytonepack')
INCOMING = ROOT / 'assets/incoming-tones/20261017/Kontakt音源-肯萨-弯音绑定.fytonepack'
TARGET = ROOT / 'assets/tone-packages/kontakt-kensa.fytonepack'
AUDIT = ROOT / 'assets/tone-packages/kensa-bend-118-audit.json'


def sha(data):
    return hashlib.sha256(data).hexdigest()


def main():
    source_bytes = SOURCE.read_bytes()
    assert sha(source_bytes) == '0ce7f0480793404dcd777bf4381fd70dcd89ddcc492e3fd2e519d316d5ac105e'
    if AUDIT.exists():
        audit = json.loads(AUDIT.read_text(encoding='utf-8'))
        assert sha(TARGET.read_bytes()) == audit['packageSha256']
        print('Kensa binding package already integrated and verified.')
        return
    with zipfile.ZipFile(SOURCE) as z:
        assert z.testzip() is None
        infos = z.infolist()
        data = {i.filename: z.read(i) for i in infos}
    manifest = json.loads(data['manifest.json'])
    for entry in manifest['files']:
        assert sha(data[entry['path']]) == entry['sha256']
    old = data['tone.xml']
    preset = ET.fromstring(old).find('PRESET')
    assert preset.get('pluginBrand') == 'kontakt'
    binding = next(x for x in preset.findall('TONE_PARAMETER') if x.get('id') == 'index:1:percent')
    assert abs(float(binding.get('value')) - 90/390) < 1e-6
    changed = old
    assert preset.get('instrumentChineseName') == 'Kontakt音源'
    name_attribute = re.search(rb'instrumentChineseName="[^"]*"', old).group(0)
    for before, after in [(b'instrumentKey="kontakt-engine"', b'instrumentKey="kontakt-kensa"'),
                          (name_attribute, 'instrumentChineseName="肯萨"'.encode())]:
        assert changed.count(before) == 1
        changed = changed.replace(before, after)
    after = ET.fromstring(changed).find('PRESET')
    assert [ET.tostring(x) for x in preset] == [ET.tostring(x) for x in after]
    data['tone.xml'] = changed
    manifest['instrument'] = '肯萨'
    for entry in manifest['files']:
        entry.update(sha256=sha(data[entry['path']]), bytes=len(data[entry['path']]))
    data['manifest.json'] = (json.dumps(manifest, ensure_ascii=False, indent=2)+'\n').encode()
    previous = sha(TARGET.read_bytes())
    INCOMING.parent.mkdir(parents=True, exist_ok=True)
    if INCOMING.exists():
        assert INCOMING.read_bytes() == source_bytes
    else:
        INCOMING.write_bytes(source_bytes)
    with zipfile.ZipFile(TARGET, 'w') as z:
        for info in infos:
            z.writestr(info, data[info.filename])
    audit = dict(sourceSha256=sha(source_bytes), packageSha256=sha(TARGET.read_bytes()),
                 previousPackageSha256=previous, source='assets/incoming-tones/20261017/'+INCOMING.name,
                 hostAutomationSlot=1, parameterName='percent', sourcePercent=100,
                 sourceNormalisedValue=float(binding.get('value')), declaredPercentRange=[10,400],
                 originalOpaqueStatePreserved=True, originalEffectsPreserved=True,
                 identityChanges=['instrumentKey: kontakt-engine -> kontakt-kensa',
                                  'instrumentChineseName/manifest instrument: Kontakt音源 -> 肯萨'],
                 windControllerButtonNotHostAutomatable=True,
                 requiresSavedWindControllerOn=True,
                 actualPitchEndpointsVerified=False)
    AUDIT.write_text(json.dumps(audit, ensure_ascii=False, indent=2)+'\n', encoding='utf-8')
    print('Integrated user binding; opaque state, effects, all curves and parameter snapshots preserved.')


if __name__ == '__main__':
    main()
