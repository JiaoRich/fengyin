"""Small, audited fix to the SHA-pinned SRL 7.0.1 WASAPI capture source.

Keep the original source archive and ship the exact diff/header alongside it.
The source hash and single-occurrence anchors prevent patching other versions.
"""
import difflib
import hashlib
import json
from pathlib import Path

SOURCE_SHA256 = 'c178fd0129dff6c9ba862730a2ac19d76489a111deef9d463acb73be0705ada0'
PATCH_ID = 'wasapi-silent-packet-1'
HEADER = Path(__file__).resolve().parents[1] / 'native/Recorder/CapturePacketCopy.h'

def patch_source(data):
    if hashlib.sha256(data).hexdigest() != SOURCE_SHA256:
        raise ValueError('Unexpected SRL WASAPI source; review the patch before building')
    original = data.decode('utf-8-sig')
    newline = '\r\n' if '\r\n' in original else '\n'
    text = original.replace('\r\n', '\n')
    replacements = [
        ('#include "WASAPICapture.h"', '#include "WASAPICapture.h"\n#include "FengYinCapturePacketCopy.h"'),
        ('\t\t\t\t\tmemset(pData, 0, sizeof(pData));',
         '\t\t\t\t\t// Silence is copied into our owned buffer below, never into the endpoint buffer.'),
        ('\t\t\t\tmemcpy_s(bufferData.get(), bufferByteCount, pData, size);',
         '\t\t\t\tif (!fengyin::capture::copyCapturePacket(bufferData.get(), bufferByteCount, pData, size,\n'
         '\t\t\t\t\t(dwFlags & AUDCLNT_BUFFERFLAGS_SILENT) != 0)) {\n'
         '\t\t\t\t\tpAudioCaptureClient->ReleaseBuffer(nNumFramesToRead);\n'
         '\t\t\t\t\treturn E_INVALIDARG;\n'
         '\t\t\t\t}'),
    ]
    for before, after in replacements:
        if text.count(before) != 1: raise ValueError('SRL patch anchor mismatch')
        text = text.replace(before, after, 1)
    return text.replace('\n', newline).encode('utf-8-sig') if data.startswith(b'\xef\xbb\xbf') else text.replace('\n', newline).encode('utf-8')

def apply(root, licenses):
    root, licenses = Path(root), Path(licenses)
    source = root / 'WASAPICapture.cpp'
    before = source.read_bytes()
    after = patch_source(before)
    source.write_bytes(after)
    header = HEADER.read_bytes()
    (root / 'FengYinCapturePacketCopy.h').write_bytes(header)
    licenses.mkdir(exist_ok=True, parents=True)
    (licenses / 'FengYinCapturePacketCopy.h').write_bytes(header)
    diff = ''.join(difflib.unified_diff(before.decode('utf-8-sig').splitlines(True),
        after.decode('utf-8-sig').splitlines(True),
        fromfile='ScreenRecorderLibNative/WASAPICapture.cpp', tofile='ScreenRecorderLibNative/WASAPICapture.cpp'))
    (licenses / 'ScreenRecorderLib-WASAPI-fix.patch').write_text(diff, encoding='utf-8')
    audit = dict(id=PATCH_ID, source='ScreenRecorderLibNative/WASAPICapture.cpp',
        beforeSha256=SOURCE_SHA256, afterSha256=hashlib.sha256(after).hexdigest(),
        headerSha256=hashlib.sha256(header).hexdigest())
    (licenses / 'ScreenRecorderLib-local-patches.json').write_text(json.dumps(audit, indent=2), encoding='utf-8')
    return audit
