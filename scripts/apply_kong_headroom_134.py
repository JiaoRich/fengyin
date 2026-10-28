"""User-approved built-in Qin output trim; all non-gain tone bytes stay exact."""
import argparse
import hashlib
import io
import json
import math
from pathlib import Path
import re
import zipfile

ROOT = Path(__file__).resolve().parents[1]
ASSETS = ROOT / 'assets/tone-packages'
BASELINE = ROOT / 'tests/fixtures/kong-gain-before-134.zip'
AUDIT = ASSETS / 'kong-headroom-134-audit.json'
TARGET_PEAK = 0.60  # At the engine's maximum supported 1.5 performance fader.
REFINE_PEAK = 0.55  # Extra reserve when full-attack validation finds a larger transient.


def sha(data):
    return hashlib.sha256(data).hexdigest()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--measurements', required=True, type=Path)
    parser.add_argument('--refine', action='store_true', help='Refine only failed verification cases from new native measurements')
    args = parser.parse_args()
    restored_audit = ASSETS / 'kong-volume-restore-137-audit.json'
    if restored_audit.exists():
        restored = json.loads(restored_audit.read_text(encoding='utf-8'))
        assert all(sha((ASSETS / r['file']).read_bytes()) == r['restoredSha256'] for r in restored['targets'])
        if args.refine:
            parser.error('Historical gain calibration retired by the user; refusing to reduce restored volumes.')
        print('Historical gain calibration retired; original volumes remain unchanged.')
        return
    prior = None
    if AUDIT.exists():
        audit = json.loads(AUDIT.read_text(encoding='utf-8'))
        assert all(sha((ASSETS / r['file']).read_bytes()) == r['afterSha256'] for r in audit['targets'])
        if not args.refine:
            print('Calibration already applied; verified no cumulative reduction.')
            return
        prior = {row['file']: row for row in audit['targets']}
        assert BASELINE.exists()
    else:
        assert not args.refine
    measurements = json.loads(args.measurements.read_text())
    assert len(measurements) == 13
    prepared = []
    for row in measurements:
        path = ASSETS / row['file']
        assert path.name == row['file'] and path.name.startswith('folk-')
        original = path.read_bytes()
        assert sha(original) == row['packageSha256']
        assert row['finite'] and row['droppedMidi'] == 0 and row['fader'] == 1.5
        with zipfile.ZipFile(io.BytesIO(original)) as package:
            assert package.testzip() is None
            infos = package.infolist()
            data = {i.filename: package.read(i) for i in infos}
        old = data['tone.xml']
        matches = list(re.finditer(rb'\boutputGain="([^"]+)"', old))
        assert len(matches) == 1
        match = matches[0]
        old_text = match.group(1).decode('ascii')
        old_gain = float(old_text)
        assert abs(old_gain - row['gain']) < 1e-6
        measured = max(row['peak'], row['oversampledPeak4x'])
        assert math.isfinite(measured) and measured > 0
        if prior:
            gain = old_gain * min(1.0, REFINE_PEAK / measured) if measured > audit['verificationCeiling'] else old_gain
        else:
            gain = old_gain * min(1.0, TARGET_PEAK / measured)
        new_text = format(gain, '.17g') if gain != old_gain else old_text
        new = old[:match.start(1)] + new_text.encode('ascii') + old[match.end(1):]
        assert new.replace(('outputGain="' + new_text + '"').encode(), match.group(0), 1) == old
        data['tone.xml'] = new
        manifest = json.loads(data['manifest.json'])
        for entry in manifest['files']:
            previous = old if entry['path'] == 'tone.xml' else data[entry['path']]
            assert len(previous) == entry['bytes'] and sha(previous) == entry['sha256']
            if entry['path'] == 'tone.xml':
                entry.update(bytes=len(new), sha256=sha(new))
        data['manifest.json'] = (json.dumps(manifest, ensure_ascii=False, indent=2) + '\n').encode('utf-8')
        output = io.BytesIO()
        with zipfile.ZipFile(output, 'w') as package:
            for info in infos:
                package.writestr(info, data[info.filename])
        result = output.getvalue()
        if new == old:
            result = original
        with zipfile.ZipFile(io.BytesIO(result)) as package:
            assert package.testzip() is None
        audit_row = {'file': path.name, 'beforeSha256': sha(original), 'afterSha256': sha(result),
                     'beforeGainText': old_text, 'afterGainText': new_text,
                     'gainDeltaDb': 20 * math.log10(gain / old_gain), 'measuredPeakBefore': row['peak'],
                     'oversampledPeak4xBefore': row['oversampledPeak4x'], 'cases': len(row['cases']),
                     'preservedEntries': {n: sha(b) for n, b in data.items() if n not in ('manifest.json', 'tone.xml')}}
        if prior:
            previous = prior[path.name]
            audit_row.update(beforeSha256=previous['beforeSha256'], beforeGainText=previous['beforeGainText'],
                             measuredPeakBefore=previous['measuredPeakBefore'], oversampledPeak4xBefore=previous['oversampledPeak4xBefore'],
                             gainDeltaDb=20 * math.log10(gain / float(previous['beforeGainText'])))
            audit_row['lastRefinement'] = {'measuredPackageSha256': sha(original), 'measuredGain': old_gain,
                                          'peak': measured, 'protocol': row['protocol']}
            audit_row['initialCases'] = previous.get('initialCases', previous['cases'])
        prepared.append((path, original, result, audit_row))
    assert len({p.name for p, *_ in prepared}) == 13
    BASELINE.parent.mkdir(parents=True, exist_ok=True)
    if not prior:
        assert not BASELINE.exists()
        with zipfile.ZipFile(BASELINE, 'w', zipfile.ZIP_DEFLATED) as backup:
            for path, original, *_ in prepared:
                backup.writestr(path.name, original)
    for path, _, result, _ in prepared:
        temporary = path.with_suffix('.gain134-tmp')
        temporary.write_bytes(result)
        temporary.replace(path)
    AUDIT.write_text(json.dumps({'version': '1.1.34', 'userAuthorized': True,
        'method': 'Existing preset outputGain only; no new limiter, compressor, AGC or per-note gain',
        'baseline': str(BASELINE.relative_to(ROOT)), 'performanceFaderTested': 1.5,
        'targetPeak': TARGET_PEAK, 'verificationCeiling': 0.63,
        'scope': '13 bundled Qin instruments; external/custom packages and accompaniment are not rewritten',
        'targets': [r for *_, r in prepared]}, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    print('Calibrated 13 built-in Qin output trims; native states and every other tone byte preserved.')


if __name__ == '__main__':
    main()
