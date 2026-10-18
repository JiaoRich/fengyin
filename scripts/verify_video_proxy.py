"""Decode the native optional playback proxy; no real-device performance claim."""
import json
from pathlib import Path
import subprocess
import sys


def verify(ffmpeg, path):
    probe=Path(ffmpeg).with_name('ffprobe.exe' if str(ffmpeg).endswith('.exe') else 'ffprobe')
    metadata=json.loads(subprocess.check_output([str(probe),'-v','error','-show_streams','-show_format','-of','json',str(path)],timeout=30))
    streams=metadata['streams']
    if len(streams)!=1 or streams[0]['codec_name']!='h264':
        raise ValueError('Proxy must contain only H264 picture; accompaniment audio stays separate')
    video=streams[0];fps=video['avg_frame_rate'].split('/')
    if video['width']!=1280 or video['height']!=720 or abs(float(fps[0])/float(fps[1])-24)>.01:
        raise ValueError('Proxy resolution or frame rate differs from test target')
    if abs(float(metadata['format']['duration'])-2)>.15:raise ValueError('Proxy timeline changed')
    picture=subprocess.check_output([str(ffmpeg),'-v','error','-i',str(path),'-an','-f','rawvideo','-pix_fmt','gray','-'],timeout=30)
    if len(picture)!=(1280*720*48):raise ValueError('Proxy did not decode to 48 frames')
    first=picture[:1280*720]
    if max(first)-min(first)<10:raise ValueError('Proxy selftest picture is unexpectedly blank')
    print('PASS actual native H264 video-only 720P/24fps cache: duration and 48 decoded frames')


if __name__=='__main__':verify(*sys.argv[1:])
