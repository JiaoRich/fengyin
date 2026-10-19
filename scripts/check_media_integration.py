"""Collect every independent Windows media failure, without a release build.

The default recorder is tested unchanged. A separate, unshipped diagnostic
target compares one upstream input-frame-rate setting. Any baseline failure
still fails this job; an experiment passing does not approve a release.
"""
import json
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]


def check(build, ffmpeg_dir, vlc):
    build, ffmpeg_dir, vlc = map(lambda p: Path(p).resolve(), (build, ffmpeg_dir, vlc))
    output = build / 'evidence'
    output.mkdir(exist_ok=True)
    results = []

    def run(name, command, timeout=90):
        try:
            process = subprocess.run([str(v) for v in command], capture_output=True, timeout=timeout)
            (output / (name + '.log')).write_bytes(process.stdout + b'\nSTDERR:\n' + process.stderr)
            result = dict(name=name, code=process.returncode)
        except subprocess.TimeoutExpired as error:
            (output / (name + '.log')).write_bytes((error.stdout or b'') + (error.stderr or b''))
            result = dict(name=name, code='timeout')
        results.append(result)
        (output / 'results.json').write_text(json.dumps(results, indent=2), encoding='utf-8')
        print(json.dumps(result), flush=True)
        return result['code'] == 0

    ffmpeg, probe = ffmpeg_dir / 'ffmpeg.exe', ffmpeg_dir / 'ffprobe.exe'
    run('capture-packet-and-viewport-policy', [build / 'Release/FengYinRecorderPolicyTests.exe'])
    for label, binary in [('legacy', 'FengYinScreenRecorder'), ('srl', 'FengYinRecorder'),
                          ('srl-explicit-rate', 'FengYinRecorderExplicitRate')]:
        exe = build / 'Release' / (binary + '.exe')
        video, audio = output / (label + '.mp4'), output / (label + '.wav')
        run(label + '-encode-video', [exe, '--selftest', video])
        run(label + '-probe', [probe, '-v', 'error', '-count_frames', '-show_streams', '-show_format', '-of', 'json', video])
        run(label + '-decode-video', [sys.executable, ROOT / 'scripts/verify_screen_recording.py', ffmpeg, video])
        run(label + '-encode-audio', [exe, '--audio-selftest', audio])
        run(label + '-decode-audio', [sys.executable, ROOT / 'scripts/verify_system_recording.py', audio])
    fixture = output / 'vlc-playback-fixture.mp4'
    if run('vlc-fixture', [ffmpeg, '-v', 'error', '-nostdin', '-y', '-stream_loop', '5', '-i', output / 'legacy.mp4', '-c', 'copy', fixture]):
        run('vlc-lifecycle', [build / 'Release/FengYinVlcTests.exe', vlc, fixture], timeout=110)
    return 1 if any(r['code'] != 0 for r in results) else 0


if __name__ == '__main__':
    sys.exit(check(*sys.argv[1:]))
