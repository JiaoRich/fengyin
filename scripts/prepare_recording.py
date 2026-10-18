"""Pinned upstream sources; no system install, no silent version upgrades."""
import hashlib
import json
from pathlib import Path, PurePosixPath
import shutil
import tempfile
import urllib.request
import zipfile
import xml.etree.ElementTree as ET

SRL_COMMIT = '646383234187efd3854e6b1069612b80c3ab9981'  # upstream 7.0.1
SRL_URL = f'https://codeload.github.com/sskodje/ScreenRecorderLib/zip/{SRL_COMMIT}'
SRL_SHA = '6bb971baa236db6374e218b0220074963a19faba5b5af6d5ff60fe0d22c4b581'
MA_VERSION = '0.11.23'
MA_URL = f'https://raw.githubusercontent.com/mackron/miniaudio/{MA_VERSION}/miniaudio.h'
MA_SHA = '7e4f3f13c8fe66df2080ac3dd12a89193e3c2463cb7f067c798abd7331cd8ee6'

def native_sources(root):
    """Follow the pinned upstream build, not every historical .cpp in its tree."""
    root=Path(root)
    project=ET.parse(root/'ScreenRecorderLibNative.vcxproj')
    ns={'ms':'http://schemas.microsoft.com/developer/msbuild/2003'}
    available={path.name.casefold():path.name for path in root.iterdir() if path.is_file()}
    sources=[]
    for item in project.findall('.//ms:ClCompile[@Include]',ns):
        name=item.attrib['Include']
        if '/' in name or '\\' in name or not name.endswith('.cpp') or any(c in name for c in '$;"'):
            raise ValueError('Unexpected upstream compile path: '+name)
        if item.find('ms:ExcludedFromBuild',ns) is not None:
            raise ValueError('Conditional upstream compilation requires explicit review: '+name)
        actual=available.get(name.casefold())
        if actual is None: raise ValueError('Missing upstream compile file: '+name)
        if actual in sources: raise ValueError('Duplicate upstream compile file: '+name)
        sources.append(actual)
    if not sources: raise ValueError('Empty upstream compile list')
    return sources

def verified(url, path, expected):
    if path is None:
        with urllib.request.urlopen(url, timeout=180) as response:
            data = response.read()
    else:
        data = Path(path).read_bytes()
    if hashlib.sha256(data).hexdigest() != expected:
        raise ValueError('Recording dependency SHA256 mismatch: ' + url)
    return data

def prepare(destination, srl_archive=None, ma_header=None):
    destination = Path(destination)
    marker = destination/'versions.json'
    versions = dict(screenRecorderLibCommit=SRL_COMMIT, screenRecorderLibSha256=SRL_SHA,
                    miniaudioVersion=MA_VERSION, miniaudioSha256=MA_SHA)
    if destination.exists() and any(destination.iterdir()):
        if not marker.exists() or json.loads(marker.read_text()) != versions:
            raise ValueError('Use an empty recording dependency directory')
    srl = verified(SRL_URL, srl_archive, SRL_SHA)
    ma = verified(MA_URL, ma_header, MA_SHA)
    destination.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='fy-recording-') as tmp:
        package = Path(tmp)/'srl.zip'; package.write_bytes(srl)
        with zipfile.ZipFile(package) as archive:
            if archive.testzip() is not None: raise ValueError('SRL CRC failure')
            for info in archive.infolist():
                parts = PurePosixPath(info.filename).parts
                if '..' in parts or info.filename.startswith('/'): raise ValueError('Unsafe SRL path')
                if info.is_dir(): continue
                # Native static library only: no CLR/UI/.NET wrapper.
                if len(parts)>1 and (parts[1]=='ScreenRecorderLibNative' or parts[1]=='LICENSE'):
                    target=destination/'srl'/Path(*parts[1:]); target.parent.mkdir(parents=True, exist_ok=True)
                    with archive.open(info) as source, target.open('wb') as out: shutil.copyfileobj(source,out)
    (destination/'miniaudio').mkdir(exist_ok=True)
    (destination/'miniaudio'/'miniaudio.h').write_bytes(ma)
    root=destination/'srl'/'ScreenRecorderLibNative'
    sources=native_sources(root)
    (root/'upstream-sources.cmake').write_text(
        '# Generated from the SHA-verified upstream vcxproj; do not glob .cpp files.\nset(SRL_SOURCES\n'
        +''.join('    "${SRL_ROOT}/'+name+'"\n' for name in sources)+')\n',encoding='utf-8')
    # Source includes upstream license texts and attribution for bundled helpers.
    (destination/'licenses').mkdir(exist_ok=True)
    shutil.copyfile(destination/'srl'/'LICENSE', destination/'licenses'/'ScreenRecorderLib-MIT.txt')
    license_text=ma.decode('utf-8').split('ALTERNATIVE 1 - Public Domain',1)[-1]
    if 'ALTERNATIVE 2 - MIT No Attribution' not in license_text: raise ValueError('miniaudio license missing')
    (destination/'licenses'/'miniaudio-license.txt').write_text('ALTERNATIVE 1 - Public Domain'+license_text, encoding='utf-8')
    (destination/'licenses'/'ScreenRecorderLib-7.0.1-source.zip').write_bytes(srl)
    (destination/'licenses'/'miniaudio-0.11.23.h').write_bytes(ma)
    marker.write_text(json.dumps(versions, indent=2), encoding='utf-8')
    print('Verified ScreenRecorderLib 7.0.1 and miniaudio 0.11.23')

if __name__ == '__main__':
    import sys
    prepare(*sys.argv[1:])
