"""Build-only VLC 3 plugin index, generated in the final private runtime tree.

Upstream bank.c reads relative plugin names plus size/mtime. Missing cache does
not write a replacement during ordinary playback. Never reset it on user PCs.
"""
import ctypes
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import time
import shutil
import tempfile


def inventory(root):
    return {str(p.relative_to(root)).replace('\\', '/'): {
        'sha256': hashlib.sha256(p.read_bytes()).hexdigest(),
        'size': p.stat().st_size, 'mtime': int(p.stat().st_mtime)}
        for p in sorted((root / 'plugins').rglob('*.dll'))}


def initialise(root, reset):
    # Executed in a bounded child process; initialise modules only, no media,
    # video window, audio endpoint, network preparsing, or user configuration.
    flags = 0x00000100 | 0x00001000
    with os.add_dll_directory(str(root)):
        core = ctypes.CDLL(str(root / 'libvlccore.dll'), winmode=flags)
        api = ctypes.CDLL(str(root / 'libvlc.dll'), winmode=flags)
        api.libvlc_new.argtypes = [ctypes.c_int, ctypes.POINTER(ctypes.c_char_p)]
        api.libvlc_new.restype = ctypes.c_void_p
        api.libvlc_release.argtypes = [ctypes.c_void_p]
        args = [b'--ignore-config', b'--no-media-library', b'--no-interact',
                b'--no-metadata-network-access', b'--quiet']
        if reset:
            args.append(b'--reset-plugins-cache')
        started = time.monotonic()
        instance = api.libvlc_new(len(args), (ctypes.c_char_p * len(args))(*args))
        if not instance:
            raise RuntimeError('Private VLC cache initialisation failed')
        api.libvlc_release(instance)
        print(json.dumps({'initialiseMs': round((time.monotonic() - started) * 1000)}))


def prepare(root):
    root = Path(root).resolve()
    before = inventory(root)
    if len(before) != 321:
        raise ValueError('Pinned x64 VLC package must contain 321 plugins')
    env = dict(os.environ)
    env.pop('VLC_PLUGIN_PATH', None)
    # Only our build helper runs with reset. Shipped player uses normal cache.
    subprocess.run([sys.executable, __file__, str(root), '--worker-reset'],
                   env=env, check=True, timeout=120)
    cache = root / 'plugins/plugins.dat'
    data = cache.read_bytes()
    if len(data) < 10000:
        raise ValueError('VLC plugin index is missing or truncated')
    for name in ('libavcodec_plugin.dll', 'libmp4_plugin.dll', 'libdirect3d11_plugin.dll'):
        if name.encode() not in data:
            raise ValueError('VLC plugin index missing ' + name)
    if before != inventory(root):
        raise ValueError('Cache generation changed plugin binaries or timestamps')
    subprocess.run([sys.executable, __file__, str(root), '--worker-read'],
                   env=env, check=True, timeout=60)
    if cache.read_bytes() != data:
        raise ValueError('Normal VLC startup unexpectedly rewrote plugin index')
    # Match installer relocation, preserving source timestamps. The original
    # directory is unavailable while the relocated copy starts in a new process.
    with tempfile.TemporaryDirectory(prefix='fy-vlc-relocation-') as tmp:
        relocated=Path(tmp)/'components/VLC'
        shutil.copytree(root,relocated,copy_function=shutil.copy2)
        hidden=root.with_name(root.name+'.cache-validation')
        root.rename(hidden)
        try:
            subprocess.run([sys.executable,__file__,str(relocated),'--worker-read'],
                           env=env,check=True,timeout=60)
            if (relocated/'plugins/plugins.dat').read_bytes()!=data:
                raise ValueError('Relocated VLC cache was modified')
        finally:
            hidden.rename(root)
    (root / 'fengyin-cache.json').write_text(json.dumps({
        'version': '3.0.24', 'sha256': hashlib.sha256(data).hexdigest(),
        'plugins': before}, indent=2) + '\n', encoding='utf-8')
    print('Verified final-layout VLC index; 321 unchanged private plugin DLLs')


if __name__ == '__main__':
    if os.name != 'nt':
        raise SystemExit('VLC Windows cache must be generated on Windows')
    if len(sys.argv) == 3 and sys.argv[2].startswith('--worker-'):
        initialise(Path(sys.argv[1]).resolve(), sys.argv[2] == '--worker-reset')
    else:
        prepare(sys.argv[1])
