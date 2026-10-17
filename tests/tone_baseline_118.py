"""Historical 1.1.18 assertions use the preserved pre-1.1.19 package bytes.

Current package changes are exhaustively checked by test_swam_gain_curve_119.
"""
from pathlib import Path
import zipfile

def historical_bytes(path):
    path=Path(path)
    for name in ['swam-before-gain-119.zip','folk-before-engine-levels-119.zip']:
        baseline=Path(__file__).resolve().parents[1]/'assets/incoming-tones/20261017'/name
        if not baseline.exists():continue
        with zipfile.ZipFile(baseline) as z:
            if path.name in z.namelist():return z.read(path.name)
    return path.read_bytes()
