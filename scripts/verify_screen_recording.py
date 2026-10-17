"""Validate the native Media Foundation encoder's actual MP4, not its exit code."""
import array
import math
from pathlib import Path
import subprocess
import sys


def verify(executable, path):
    def decode(*args):
        return subprocess.run([str(executable), '-v', 'error', '-i', str(path), *args],
                              stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                              check=True, timeout=30).stdout
    if Path(path).stat().st_size < 4096:
        raise ValueError('MP4 is empty')
    frame = decode('-frames:v', '1', '-an', '-f', 'rawvideo', '-pix_fmt', 'rgb24', '-')
    if len(frame) != 320*240*3:
        raise ValueError('Unexpected encoded frame dimensions')
    top = frame[(40*320+160)*3: (40*320+160)*3+3]
    bottom = frame[(200*320+160)*3: (200*320+160)*3+3]
    if not (top[0] > 180 and top[2] < 60 and bottom[2] > 180 and bottom[0] < 60):
        raise ValueError(f'Video orientation or colour wrong: top={list(top)}, bottom={list(bottom)}')
    all_frames = decode('-an', '-f', 'rawvideo', '-pix_fmt', 'gray', '-')
    if len(all_frames)//(320*240) != 48:
        raise ValueError('MP4 frame count is not 48')
    pcm = array.array('h', decode('-vn', '-f', 's16le', '-ac', '2', '-ar', '48000', '-'))
    if not (1.95 <= len(pcm)/(48000*2) <= 2.1):
        raise ValueError('Encoded audio duration is not two seconds')
    rms = math.sqrt(sum(v*v for v in pcm)/len(pcm))
    if not 1000 < rms < 3000:
        raise ValueError(f'Encoded audio is silent/corrupt: RMS={rms}')
    print('Native MP4: H264 image orientation, 48 frames, stereo audio duration and non-silence passed')


if __name__ == '__main__':
    verify(*sys.argv[1:])
