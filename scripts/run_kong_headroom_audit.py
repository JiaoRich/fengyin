"""Offline native-plugin audit; source packages are read, never rewritten here."""
import argparse
import concurrent.futures
import hashlib
import json
from pathlib import Path
import subprocess

import numpy as np
import soundfile as sf
from scipy.signal import resample_poly

ROOT = Path(__file__).resolve().parents[1]


def oversampled_peak(path):
    peak = 0.0
    with sf.SoundFile(path) as audio:
        assert audio.subtype == 'FLOAT', 'Audit must preserve peaks above 0 dBFS'
        frames = len(audio)
        for start in range(0, frames, 262144 - 128):
            audio.seek(start)
            data = audio.read(262144, dtype='float64', always_2d=True)
            assert np.isfinite(data).all()
            expanded = resample_poly(data, 4, 1, axis=0, window=('kaiser', 8.6))
            lo = 256 if start else 0
            hi = -256 if start + len(data) < frames else None
            if expanded[lo:hi].size:
                peak = max(peak, float(np.max(np.abs(expanded[lo:hi]))))
            if start + len(data) >= frames:
                break
    return peak


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--helper', required=True, type=Path)
    parser.add_argument('--plugin', required=True, type=Path)
    parser.add_argument('--output', required=True, type=Path)
    parser.add_argument('--workers', type=int, default=2)
    parser.add_argument('--rate', type=int, default=44100, choices=[44100, 48000, 96000])
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    paths = sorted((ROOT / 'assets/tone-packages').glob('folk-*.fytonepack'))
    assert len(paths) == 13

    def audit(path):
        dest = args.output / path.stem
        dest.mkdir(parents=True, exist_ok=True)
        digest = hashlib.sha256(path.read_bytes()).hexdigest()
        metrics = dest / 'metrics.json'
        old = json.loads(metrics.read_text()) if metrics.exists() else {}
        if old.get('packageSha256') != digest or old.get('sampleRate',44100) != args.rate or old.get('protocol') != 2:
            result = subprocess.run([str(args.helper), str(args.plugin), str(path), str(dest.resolve()),str(args.rate)],
                                    capture_output=True, text=True, timeout=600)
            (dest / 'run.log').write_text(result.stdout + result.stderr)
            if result.returncode:
                raise RuntimeError(f'{path.name}: native audit exited {result.returncode}: {result.stderr[-1000:]}')
        row = json.loads(metrics.read_text())
        assert row['finite'] and row['droppedMidi'] == 0 and row['peak'] > 0
        row['packageSha256'] = digest
        row['file'] = path.name
        row['oversampledPeak4x'] = oversampled_peak(dest / 'sweep-float.wav')
        metrics.write_text(json.dumps(row, ensure_ascii=False, indent=2) + '\n')
        print(f"{path.name}: {len(row['cases'])} cases, peak={row['peak']:.5f}, 4x={row['oversampledPeak4x']:.5f}", flush=True)
        return row

    with concurrent.futures.ThreadPoolExecutor(max_workers=args.workers) as pool:
        rows = list(pool.map(audit, paths))
    (args.output / 'audit.json').write_text(json.dumps(rows, ensure_ascii=False, indent=2) + '\n')
    print('Completed all 13 packages', flush=True)


if __name__ == '__main__':
    main()
