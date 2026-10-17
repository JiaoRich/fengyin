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
    width,height=1920,1080
    if len(frame) != width*height*3:
        raise ValueError('Unexpected encoded frame dimensions')
    top = frame[(100*width+width//2)*3: (100*width+width//2)*3+3]
    bottom = frame[(900*width+width//2)*3: (900*width+width//2)*3+3]
    if not (top[0] > 180 and top[2] < 60 and bottom[2] > 180 and bottom[0] < 60):
        raise ValueError(f'Video orientation or colour wrong: top={list(top)}, bottom={list(bottom)}')
    all_frames = decode('-an', '-f', 'rawvideo', '-pix_fmt', 'gray', '-')
    if len(all_frames)//(width*height) != 48:
        raise ValueError('MP4 frame count is not 48')
    pcm = array.array('h', decode('-vn', '-f', 's16le', '-ac', '2', '-ar', '48000', '-'))
    if not (1.95 <= len(pcm)/(48000*2) <= 2.1):
        raise ValueError('Encoded audio duration is not two seconds')
    rms = math.sqrt(sum(v*v for v in pcm)/len(pcm))
    if not 1000 < rms < 3000:
        raise ValueError(f'Encoded audio is silent/corrupt: RMS={rms}')
    # Ignore encoder priming/tail, then reject intermittent silent blocks that
    # a global RMS check would miss. This remains an encoder/timeline selftest,
    # not a claim that live WGC/WASAPI passed on the target computer.
    interior=pcm[8192:-8192]
    for start in range(0,len(interior)-1920,1920):
        block=interior[start:start+1920]
        local_rms=math.sqrt(sum(v*v for v in block)/len(block))
        if local_rms<1000:raise ValueError(f'Audio discontinuity near sample {start}: RMS={local_rms}')
    print('Native MP4: 1920x1080 H264 orientation, 48 frames, stereo duration and continuous audio passed')


if __name__ == '__main__':
    verify(*sys.argv[1:])
