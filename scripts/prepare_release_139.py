"""One approved native transpose edit and removal from the built-in inventory.

No gain, controller curve, effect or other tuner parameter is changed.
"""
from pathlib import Path
import hashlib, json, re, struct, zipfile
import xml.etree.ElementTree as ET
from apply_swam_gain_curve_119 import decode, encode, xml_bytes, replace_xml

ROOT = Path(__file__).resolve().parents[1]
ASSETS = ROOT / 'assets/tone-packages'
def sha(raw): return hashlib.sha256(raw).hexdigest()

def patch_transpose(state):
    outer = decode(state); ox = xml_bytes(outer)
    component = ET.fromstring(ox.rstrip(b'\0')).findtext('IComponent')
    body = decode(component); chunk = body[176:]; ix = xml_bytes(chunk)
    before = ET.fromstring(ix.rstrip(b'\0'))
    target = [p for p in before.iter('PARAM') if p.get('id') == 'transpose']
    assert len(target) == 1 and float(target[0].get('value')) == 12
    changed, n = re.subn(rb'(<PARAM\s+id="transpose"\s+value=")12\.0(")', rb'\g<1>0.0\2', ix)
    assert n == 1
    after = ET.fromstring(changed.rstrip(b'\0')); target[0].set('value', '0.0')
    assert ET.tostring(before) == ET.tostring(after), 'Unexpected native parameter change'
    new_chunk = replace_xml(chunk, changed)
    new_body = bytearray(body[:176] + new_chunk)
    struct.pack_into('>I', new_body, 20, len(new_body)-24)
    struct.pack_into('>I', new_body, 172, len(new_chunk))
    assert ox.count(component.encode()) == 1
    return encode(replace_xml(outer, ox.replace(component.encode(), encode(new_body).encode())))

def main():
    audit_path = ASSETS/'release-139-audit.json'
    if audit_path.exists():
        audit = json.loads(audit_path.read_text(encoding='utf-8'))
        assert all(sha((ASSETS/r['asset']).read_bytes()) == r['sha256'] for r in audit['tones'])
        print('Release139 assets already prepared and verified.'); return
    previous = json.loads((ASSETS/'release-137-audit.json').read_text(encoding='utf-8'))
    path = ASSETS/'trumpet.fytonepack'; original_sha = sha(path.read_bytes())
    with zipfile.ZipFile(path) as z:
        infos = z.infolist(); data = {i.filename:z.read(i) for i in infos}; assert z.testzip() is None
    original = data['tone.xml']; p = ET.fromstring(original).find('PRESET')
    old_state = p.findtext('INSTRUMENT_STATE'); new_state = patch_transpose(old_state)
    changed = original.replace(old_state.encode(), new_state.encode())
    assert original.count(old_state.encode()) == 1
    a = ET.fromstring(original); b = ET.fromstring(changed)
    a.find('PRESET/INSTRUMENT_STATE').text = b.findtext('PRESET/INSTRUMENT_STATE')
    assert ET.tostring(a) == ET.tostring(b), 'Unexpected package tuning change'
    data['tone.xml'] = changed
    manifest = json.loads(data['manifest.json'])
    for row in manifest['files']: row.update(bytes=len(data[row['path']]), sha256=sha(data[row['path']]))
    data['manifest.json'] = (json.dumps(manifest, ensure_ascii=False, indent=2)+'\n').encode()
    with zipfile.ZipFile(path.with_suffix('.tmp'), 'w') as z:
        for info in infos: z.writestr(info, data[info.filename])
    path.with_suffix('.tmp').replace(path)
    deleted = next(r for r in previous['tones'] if r['id'] == 'kontakt-kensa-2')
    removed=ASSETS/deleted['asset']
    if removed.exists():
        assert sha(removed.read_bytes())==deleted['sha256']
        removed.unlink()
    rows = [dict(r) for r in previous['tones'] if r['id'] != 'kontakt-kensa-2']
    for row in rows:
        if row['id'] == 'trumpet':
            row.update(sha256=sha(path.read_bytes()), statePreserved=False, statePreservedExcept=['native transpose 12 to 0'])
        assert sha((ASSETS/row['asset']).read_bytes()) == row['sha256']
    assert len(rows) == 41
    audit = {'version':'1.1.39', 'calibration':'deferred', 'derivedFrom':'release-137-audit.json',
        'approvedChanges':[{'id':'trumpet', 'nativeParameter':'transpose', 'before':12, 'after':0,
          'beforeSha256':original_sha, 'afterSha256':sha(path.read_bytes()), 'allOtherTuningPreserved':True},
          {'id':deleted['id'], 'action':'remove_builtin_only', 'asset':deleted['asset'], 'sha256':deleted['sha256']}], 'tones':rows}
    audit_path.write_text(json.dumps(audit, ensure_ascii=False, indent=2)+'\n')
    print('41 tones verified; only trumpet1 native transpose changed; Kensa2 removed from built-in list.')
if __name__ == '__main__': main()
