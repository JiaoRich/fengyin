"""Release miniaudio-only dependency; archived screen capture is not downloaded."""
import hashlib
import json
from pathlib import Path
import urllib.request

VERSION = '0.11.23'
SHA = '7e4f3f13c8fe66df2080ac3dd12a89193e3c2463cb7f067c798abd7331cd8ee6'
URL = f'https://raw.githubusercontent.com/mackron/miniaudio/{VERSION}/miniaudio.h'


def prepare(destination, header=None):
    destination = Path(destination)
    target = destination/'miniaudio'/'miniaudio.h'
    if header is not None:
        data = Path(header).read_bytes()
    elif target.exists():
        data = target.read_bytes()
    else:
        with urllib.request.urlopen(URL, timeout=180) as response:
            data = response.read()
    if hashlib.sha256(data).hexdigest() != SHA:
        raise ValueError('miniaudio source SHA256 mismatch')
    text = data.decode('utf-8')
    if 'ALTERNATIVE 1 - Public Domain' not in text or 'ALTERNATIVE 2 - MIT No Attribution' not in text:
        raise ValueError('miniaudio license missing')
    licenses = destination/'licenses'
    # Use a dedicated clean release directory. Never silently package archived
    # ScreenRecorderLib licenses/sources left by an earlier full-media build.
    if licenses.exists() and any(p.name.startswith('ScreenRecorderLib') for p in licenses.iterdir()):
        raise ValueError('Use a clean audio-only dependency directory')
    target.parent.mkdir(parents=True, exist_ok=True)
    licenses.mkdir(parents=True, exist_ok=True)
    target.write_bytes(data)
    (licenses/f'miniaudio-{VERSION}.h').write_bytes(data)
    (licenses/'miniaudio-license.txt').write_text(
        'ALTERNATIVE 1 - Public Domain'+text.split('ALTERNATIVE 1 - Public Domain', 1)[1], encoding='utf-8')
    (destination/'versions.json').write_text(json.dumps({
        'audioOnly': True, 'miniaudioVersion': VERSION, 'miniaudioSha256': SHA,
    }, indent=2)+'\n', encoding='utf-8')
    print('Verified audio-only miniaudio '+VERSION)


if __name__ == '__main__':
    import sys
    prepare(sys.argv[1], sys.argv[2] if len(sys.argv)>2 else None)
