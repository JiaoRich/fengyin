"""Restore the user's pre-1.1.34 Qin volumes, preserving all other tone state."""
import hashlib
import io
import json
from pathlib import Path
import zipfile

ROOT = Path(__file__).resolve().parents[1]
ASSETS = ROOT / 'assets/tone-packages'
AUDIT = ASSETS / 'kong-volume-restore-137-audit.json'
HISTORY = ROOT / 'tests/fixtures/kong-calibrated-before-137.zip'


def sha(data):
    return hashlib.sha256(data).hexdigest()


def main():
    if AUDIT.exists():
        audit = json.loads(AUDIT.read_text(encoding='utf-8'))
        assert all(sha((ASSETS / r['file']).read_bytes()) == r['restoredSha256'] for r in audit['targets'])
        print('Original Qin volumes already restored; no cumulative changes.')
        return
    old = json.loads((ASSETS / 'kong-headroom-134-audit.json').read_text(encoding='utf-8'))
    prepared = []
    with zipfile.ZipFile(ROOT / old['baseline']) as baseline:
        for r in old['targets']:
            current = (ASSETS / r['file']).read_bytes()
            restored = baseline.read(r['file'])
            assert sha(current) == r['afterSha256'] and sha(restored) == r['beforeSha256']
            with zipfile.ZipFile(io.BytesIO(current)) as a, zipfile.ZipFile(io.BytesIO(restored)) as b:
                assert a.testzip() is None and b.testzip() is None and a.namelist() == b.namelist()
                tone = a.read('tone.xml').replace(('outputGain="'+r['afterGainText']+'"').encode(),
                                                ('outputGain="'+r['beforeGainText']+'"').encode(), 1)
                assert tone == b.read('tone.xml')
                for name in a.namelist():
                    if name not in ('tone.xml', 'manifest.json'):
                        assert a.read(name) == b.read(name)
            prepared.append((r, current, restored))
    assert len(prepared) == 13 and not HISTORY.exists()
    with zipfile.ZipFile(HISTORY, 'w', zipfile.ZIP_DEFLATED) as history:
        for r, current, _ in prepared:
            history.writestr(r['file'], current)
    for r, _, restored in prepared:
        dest = ASSETS / r['file']
        temporary = dest.with_suffix('.restore137-tmp')
        temporary.write_bytes(restored)
        temporary.replace(dest)
    audit = {'version': '1.1.37', 'userRequested': 'Restore original Qin instrument volumes; keep exclusive CH1/CH2 routing.',
             'previousCalibration': 'kong-headroom-134-audit.json', 'restoredBaseline': old['baseline'],
             'calibratedArchive': str(HISTORY.relative_to(ROOT)),
             'scope': '13 bundled Qin outputGain values only; plugin states, mappings, effects and other instruments unchanged.',
             'measurementScope': '1.1.34/1.1.36 headroom measurements describe the archived reduced-gain packages, not these restored volumes.',
             'targets': [{'file': r['file'], 'calibratedSha256': sha(current), 'restoredSha256': sha(restored),
                          'calibratedGainText': r['afterGainText'], 'restoredGainText': r['beforeGainText']}
                         for r, current, restored in prepared]}
    AUDIT.write_text(json.dumps(audit, ensure_ascii=False, indent=2)+'\n', encoding='utf-8')
    print('Restored 13 original Qin volumes exactly; all other tone bytes preserved.')


if __name__ == '__main__':
    main()
