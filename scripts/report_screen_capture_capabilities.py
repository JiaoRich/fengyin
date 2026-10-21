"""Record GPU capability separately from CI/real-instrument acceptance."""
import json
from pathlib import Path
import subprocess
import sys


def report(helper, directory):
    helper = Path(helper).resolve()
    directory = Path(directory).resolve()
    directory.mkdir(parents=True, exist_ok=True)
    results = {}
    for name, arguments in [('gpuConversion', ['--gpu-selftest']),
                            ('hardwareEncoder', ['--encode-selftest', str(directory/'hardware.mp4')])]:
        result = subprocess.run([str(helper), *arguments], capture_output=True, timeout=60)
        (directory/(name+'.stdout.txt')).write_bytes(result.stdout)
        (directory/(name+'.stderr.txt')).write_bytes(result.stderr)
        results[name+'Exit'] = result.returncode
    results.update(targetPerformanceAccepted=False, hardwareCaptureAccepted=False)
    (directory/'capabilities.json').write_text(json.dumps(results, indent=2)+'\n', encoding='utf-8')
    print(json.dumps(results))


if __name__ == '__main__':
    report(*sys.argv[1:])
