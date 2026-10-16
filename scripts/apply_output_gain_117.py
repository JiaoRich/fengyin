"""Apply the user-approved fixed +6 dB trim, without touching plugin states/curves."""
import hashlib
import json
import math
import re
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PACKAGES = ROOT / 'assets/tone-packages'
AUDIT = PACKAGES / 'output-gain-117-audit.json'
TARGETS = ['flute', 'piccolo', 'clarinet', 'oboe', 'violin-soft'] + [
    'folk-' + name for name in ('erhu', 'hulusi', 'liuqin', 'matouqin', 'qudi',
    'xun', 'suona', 'sanxian', 'pipa', 'nanxiao', 'dongxiao', 'bangdi', 'jinghu')]


def digest(data):
    return hashlib.sha256(data).hexdigest()


def main():
    if AUDIT.exists():
        audit = json.loads(AUDIT.read_text(encoding='utf-8'))
        assert all(digest((PACKAGES / row['file']).read_bytes()) == row['afterPackageSha256'] for row in audit['targets'])
        print('Already applied and verified; no second boost.')
        return
    prepared = []
    for stem in TARGETS:
        path = PACKAGES / (stem + '.fytonepack')
        with zipfile.ZipFile(path) as z:
            assert z.testzip() is None
            infos = z.infolist()
            payloads = {info.filename: z.read(info) for info in infos}
        old_tone = payloads['tone.xml']
        matches = list(re.finditer(rb'\boutputGain="([^"]+)"', old_tone))
        assert len(matches) == 1, stem
        old_value = matches[0].group(1).decode('ascii')
        before = float(old_value)
        assert before == 1.0, (stem, before)  # Never overwrite an unexpected tuner level.
        after = before * math.pow(10, 6 / 20)
        assert 0 < after <= 2
        new_value = format(after, '.17g')
        new_tone = old_tone[:matches[0].start(1)] + new_value.encode('ascii') + old_tone[matches[0].end(1):]
        manifest = json.loads(payloads['manifest.json'])
        for entry in manifest['files']:
            assert digest(payloads[entry['path']]) == entry['sha256']
            if entry['path'] == 'tone.xml':
                entry['bytes'] = len(new_tone)
                entry['sha256'] = digest(new_tone)
        row = {'file': path.name, 'db': 6, 'beforeGainText': old_value, 'afterGainText': new_value,
               'beforePackageSha256': digest(path.read_bytes()), 'beforeToneSha256': digest(old_tone),
               'preservedEntries': {name: digest(data) for name, data in payloads.items() if name not in ('tone.xml', 'manifest.json')}}
        payloads['tone.xml'] = new_tone
        payloads['manifest.json'] = (json.dumps(manifest, ensure_ascii=False, indent=2) + '\n').encode('utf-8')
        prepared.append((path, infos, payloads, row))
    # All sources checked before any replacement. Keep original user intake files untouched.
    backup = ROOT / 'outputs/output-gain-117-before'
    backup.mkdir(parents=True, exist_ok=True)
    for path, infos, payloads, row in prepared:
        original = path.read_bytes()
        (backup / path.name).write_bytes(original)
        temporary = path.with_suffix('.fytonepack.gain117-tmp')
        with zipfile.ZipFile(temporary, 'w') as z:
            for info in infos:
                z.writestr(info, payloads[info.filename])
        with zipfile.ZipFile(temporary) as z:
            assert z.testzip() is None
        temporary.replace(path)
        row['afterPackageSha256'] = digest(path.read_bytes())
    AUDIT.write_text(json.dumps({'db': 6, 'method': 'fixed preset outputGain; original plugin state/curves preserved',
        'targets': [row for _, _, _, row in prepared]}, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    print('Applied exact +6 dB to', len(prepared), 'packages; originals backed up under', backup)


if __name__ == '__main__':
    main()
