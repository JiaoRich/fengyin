"""Real media checks for the isolated recorder; never target performance claims."""
import array
import hashlib
import json
import math
from pathlib import Path
import subprocess
import sys


def check(helper, ffmpeg_dir, directory):
    helper, ffmpeg_dir, directory = Path(helper).resolve(), Path(ffmpeg_dir).resolve(), Path(directory).resolve()
    directory.mkdir(parents=True, exist_ok=True)
    suffix = '.exe' if sys.platform == 'win32' else ''
    ffmpeg, ffprobe = ffmpeg_dir / ('ffmpeg' + suffix), ffmpeg_dir / ('ffprobe' + suffix)
    video, audio, output = directory / 'fixture.mp4', directory / 'fixture.wav', directory / 'muxed.mp4'

    def run(args, **kwargs):
        return subprocess.run([str(a) for a in args], check=True, capture_output=True, timeout=120, **kwargs).stdout

    fixture_encoder = None
    for encoder in ('libopenh264', 'h264_mf'):
        attempt = subprocess.run([str(ffmpeg), '-v', 'error', '-nostdin', '-y', '-f', 'lavfi', '-i',
                                  'testsrc2=s=1280x720:r=30', '-t', '2', '-c:v', encoder, str(video)],
                                 capture_output=True, timeout=120)
        if attempt.returncode == 0:
            fixture_encoder = encoder
            break
    assert fixture_encoder, 'No H264 fixture encoder; this is not a skipped mux test'
    run([ffmpeg, '-v', 'error', '-nostdin', '-y', '-f', 'lavfi', '-i',
         'aevalsrc=0.18*sin(2*PI*440*t)+0.08*sin(2*PI*997*t):s=48000:d=2.125',
         '-ac', '2', '-c:a', 'pcm_s16le', audio])
    run([helper, '--mux-existing', video, audio, output, '1250000', '20000000'])

    def hashes(path):
        data = json.loads(run([ffprobe, '-v', 'error', '-select_streams', 'v:0', '-show_packets',
                               '-show_data_hash', 'sha256', '-show_entries', 'packet=data_hash', '-of', 'json', path]))
        return [p['data_hash'] for p in data['packets']]

    source_hashes, result_hashes = hashes(video), hashes(output)
    assert len(source_hashes) == 60 and source_hashes == result_hashes, 'Video was changed or frames lost during mux'
    metadata = json.loads(run([ffprobe, '-v', 'error', '-show_streams', '-of', 'json', output]))
    stream = next(s for s in metadata['streams'] if s['codec_type'] == 'audio')
    assert stream['codec_name'] == 'aac' and stream['sample_rate'] == '48000' and stream['channels'] == 2
    raw = run([ffmpeg, '-v', 'error', '-i', output, '-vn', '-f', 's16le', '-ac', '2', '-ar', '48000', '-'])
    samples = array.array('h', raw)
    assert 96000 <= len(samples) // 2 <= 97024, 'Unexpected decoded audio duration'
    # AAC is lossy. Compare a stable central region with the explicitly trimmed
    # original; do not call compressed output lossless or ignore discontinuities.
    expected = array.array('h', run([ffmpeg, '-v', 'error', '-i', audio, '-af', 'atrim=start_sample=6000:end_sample=102000,asetpts=PTS-STARTPTS',
                                    '-f', 's16le', '-ac', '2', '-ar', '48000', '-']))
    begin, end = 8192 * 2, 88000 * 2
    signal = sum(float(v) ** 2 for v in expected[begin:end])
    noise = sum(float(a - b) ** 2 for a, b in zip(expected[begin:end], samples[begin:end]))
    snr = 10 * math.log10(signal / max(1, noise))
    assert snr > 25, f'Trim/alignment or AAC quality regression: SNR {snr}'
    before = hashlib.sha256(output.read_bytes()).hexdigest()
    refused = subprocess.run([str(helper), '--mux-existing', str(video), str(audio), str(output), '0', '20000000'], capture_output=True, timeout=120)
    assert refused.returncode != 0 and hashlib.sha256(output.read_bytes()).hexdigest() == before
    short = subprocess.run([str(helper), '--mux-existing', str(video), str(audio), str(directory / 'insufficient.mp4'), '1250000', '30000000'], capture_output=True, timeout=120)
    assert short.returncode != 0, 'Missing audio was silently padded'
    result = dict(fixtureEncoder=fixture_encoder, videoPackets=60, compressedVideoUnchanged=True, audioDecodedFrames=len(samples)//2,
                  audioSnrDb=snr, overwriteRefused=True, insufficientAudioRefused=True,
                  targetPerformanceAccepted=False, hardwareCaptureAccepted=False)
    (directory / 'mux-verification.json').write_text(json.dumps(result, indent=2) + '\n', encoding='utf-8')
    print(json.dumps(result))


if __name__ == '__main__':
    check(*sys.argv[1:])
