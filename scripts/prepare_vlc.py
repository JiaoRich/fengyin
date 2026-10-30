"""Pinned, application-private VideoLAN runtime + headers, never a system install."""
import hashlib
import json
from pathlib import Path, PurePosixPath
import shutil
import tempfile
import tarfile
import urllib.request
import zipfile
import os
from datetime import datetime, timezone

VERSION = '3.0.24'
URL = f'https://api.nuget.org/v3-flatcontainer/videolan.libvlc.windows/{VERSION}/videolan.libvlc.windows.{VERSION}.nupkg'
SHA256 = '3da3f37ee2b2b10e5c788d830a3b513658c36521ea1b0f966ed86eb69f41ef3c'
SOURCE_URL = f'https://download.videolan.org/pub/videolan/vlc/{VERSION}/vlc-{VERSION}.tar.xz'
SOURCE_SHA256 = 'e7cab503d1d7d5849b89d2cf0e1ee60d0ef6d012407791b644b9cfc0cc225fdf'

def prepare(destination, archive=None, source_archive=None):
    destination=Path(destination)
    if destination.exists() and any(destination.iterdir()):
        marker=destination/'fengyin-source.json'
        if not marker.exists() or json.loads(marker.read_text(encoding='utf-8')).get('sha256')!=SHA256:
            raise ValueError('Use an empty VLC destination; do not mix distributions or versions')
    with tempfile.TemporaryDirectory(prefix='fengyin-vlc-') as tmp:
        if archive is None:
            archive=Path(tmp)/'vlc.zip'
            with urllib.request.urlopen(URL,timeout=120) as source, archive.open('wb') as out:
                shutil.copyfileobj(source,out)
        archive=Path(archive)
        if hashlib.sha256(archive.read_bytes()).hexdigest()!=SHA256:
            raise ValueError('LibVLC SHA256 mismatch')
        destination.mkdir(parents=True,exist_ok=True)
        with zipfile.ZipFile(archive) as package:
            if package.testzip() is not None: raise ValueError('LibVLC archive CRC failure')
            for info in package.infolist():
                parts=PurePosixPath(info.filename).parts
                if '..' in parts or info.filename.startswith('/'): raise ValueError('Unsafe VLC path')
                if info.is_dir():continue
                if parts[:2]!=('build','x64'):continue
                relative=Path(*parts[2:])
                if parts[2]=='include':relative=Path('sdk')/relative
                # Official LGPL package, not the separate Windows.GPL package.
                target=destination/relative;target.parent.mkdir(parents=True,exist_ok=True)
                with package.open(info) as source,target.open('wb') as out:shutil.copyfileobj(source,out)
                # VLC's plugin index stores mtime. Keep extraction deterministic
                # across fresh CI trees and copies into the installer layout.
                timestamp=datetime(*info.date_time,tzinfo=timezone.utc).timestamp()
                os.utime(target,(timestamp,timestamp))
        if source_archive is None:
            source_archive=Path(tmp)/'vlc-source.tar.xz'
            with urllib.request.urlopen(SOURCE_URL,timeout=120) as source,source_archive.open('wb') as out:
                shutil.copyfileobj(source,out)
        source_archive=Path(source_archive)
        if hashlib.sha256(source_archive.read_bytes()).hexdigest()!=SOURCE_SHA256:
            raise ValueError('LibVLC source SHA256 mismatch')
        with tarfile.open(source_archive,'r:xz') as source:
            for entry in source.getmembers():
                path=PurePosixPath(entry.name)
                if not entry.isfile() or '..' in path.parts:continue
                relative=path.parts[1:]
                if len(relative)==1 and relative[0].startswith('COPYING'):
                    target=destination/'sdk'/Path(*relative)
                    target.parent.mkdir(parents=True,exist_ok=True)
                    with source.extractfile(entry) as data,target.open('wb') as out:shutil.copyfileobj(data,out)
        # Supply exact upstream source alongside its notices, not just a dead URL.
        (destination/'source').mkdir(exist_ok=True)
        shutil.copyfile(source_archive,destination/'source'/f'vlc-{VERSION}.tar.xz')
        shutil.copyfile(destination/'sdk'/'COPYING.LIB',destination/'COPYING.txt')
        for name in ('libvlc.dll','libvlccore.dll','sdk/include/vlc/vlc.h','plugins/codec/libavcodec_plugin.dll','COPYING.txt'):
            if not (destination/name).is_file():raise ValueError(f'LibVLC component missing: {name}')
        (destination/'fengyin-source.json').write_text(json.dumps(dict(version=VERSION,url=URL,sha256=SHA256,
            source=SOURCE_URL,sourceSha256=SOURCE_SHA256),indent=2),encoding='utf-8')
        print('Verified LibVLC',VERSION,SHA256)

if __name__=='__main__':
    import sys
    prepare(sys.argv[1],sys.argv[2] if len(sys.argv)>2 else None,sys.argv[3] if len(sys.argv)>3 else None)
