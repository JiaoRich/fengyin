"""Run both real Windows window strategies; preserve failures and keep testing."""
import json
import os
from pathlib import Path
import subprocess
import sys

root=Path(sys.argv[1]).resolve()
fixture=Path(sys.argv[2]).resolve()
evidence=root/'evidence'
evidence.mkdir(exist_ok=True)
results=[]
for mode in ('immediate','deferred'):
    directory=evidence/mode
    directory.mkdir(exist_ok=True)
    with (directory/'run.log').open('wb') as log:
        process=subprocess.Popen([str(root/'Release/FengYinVlcWebViewCheck.exe'),str(fixture),mode],
                                 cwd=directory,stdout=log,stderr=subprocess.STDOUT)
        timeout=False
        try:
            code=process.wait(timeout=150)
        except subprocess.TimeoutExpired:
            timeout=True
            subprocess.run(['taskkill','/PID',str(process.pid),'/T','/F'],capture_output=True)
            process.wait();code=-1
    text=(directory/'run.log').read_text(encoding='utf-8',errors='replace')
    # A zero exit without twenty actual HWND -> WebView -> JS -> native input
    # acknowledgements is not a pass.
    acknowledgements=text.count('PASS physical click cycle=')
    passed=code==0 and acknowledgements==20 and not timeout
    results.append(dict(mode=mode,exitCode=code,timedOut=timeout,clickAcknowledgements=acknowledgements,passed=passed))
    print(json.dumps(results[-1]),flush=True)
(evidence/'results.json').write_text(json.dumps(results,indent=2)+'\n',encoding='utf-8')
sys.exit(0 if all(r['passed'] for r in results) else 1)
