"""Migrate bundled SWAM packages; preserve opaque state and all other parameters."""
import hashlib
import json
from pathlib import Path
import re
import zipfile

ROOT = Path(__file__).resolve().parents[1]

def migrate(path):
    with zipfile.ZipFile(path) as archive:
        entries = [(info, archive.read(info.filename)) for info in archive.infolist()]
    data = dict((info.filename, payload) for info, payload in entries)
    original = data['tone.xml']
    updated, count = re.subn(
        rb'(<TONE_PARAMETER\s+id="index:\d+:attackcontrol"\s+value=")[^"]+("/>)',
        rb'\g<1>1.0\2', original)
    if count != 1:
        raise ValueError(f'{path}: expected exactly one Attack Control, got {count}')
    if updated == original:
        return
    manifest = json.loads(data['manifest.json'])
    record = next(item for item in manifest['files'] if item['path'] == 'tone.xml')
    record['sha256'] = hashlib.sha256(updated).hexdigest()
    record['bytes'] = len(updated)
    data['tone.xml'] = updated
    data['manifest.json'] = json.dumps(manifest, ensure_ascii=False, indent=2).encode('utf-8')
    temporary = path.with_suffix('.attack-update.tmp')
    with zipfile.ZipFile(temporary, 'w') as archive:
        for info, _ in entries:
            archive.writestr(info, data[info.filename])
    with zipfile.ZipFile(temporary) as archive:
        assert archive.testzip() is None
        assert archive.read('tone.xml') == updated
    temporary.replace(path)
    print(f'Updated {path.relative_to(ROOT)}')

if __name__ == '__main__':
    for path in sorted((ROOT / 'assets/tone-packages').glob('*.fytonepack')):
        migrate(path)
