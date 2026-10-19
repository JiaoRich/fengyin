"""Real Windows UI/video checks with no-player controls and reversed ordering."""
import argparse
import json
from pathlib import Path
import subprocess
import sys
import uuid


def case_order(rounds):
    modes=('immediate','deferred','attached-immediate','attached-deferred')
    for iteration in range(rounds):
        ordered=modes if iteration%2==0 else tuple(reversed(modes))
        for mode in ('web-only',*ordered):
            yield iteration+1,mode


def check_result(mode,code,timeout,text):
    acknowledgements=text.count('PASS physical click cycle=')
    previews=text.count('PASS real paused first picture generation=')
    passed=(code==0 and acknowledgements==20 and not timeout
            and (mode=='web-only' or (previews>=4 if mode=='production-ui' else previews==4)))
    return dict(mode=mode,exitCode=code,timedOut=timeout,
                clickAcknowledgements=acknowledgements,pausedFirstPictures=previews,passed=passed)


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('root',type=Path)
    parser.add_argument('fixture',type=Path,help='red video with nonzero audio, not arbitrary customer media')
    parser.add_argument('--rounds',type=int,choices=(1,2),default=2)
    args=parser.parse_args()
    root=args.root.resolve();fixture=args.fixture.resolve()
    evidence=root/'evidence';evidence.mkdir(exist_ok=True)
    session=evidence/('run-'+uuid.uuid4().hex[:12]);session.mkdir()
    results=[]
    for iteration,mode in (*case_order(args.rounds),(args.rounds+1,'production-ui')):
        directory=session/f'{iteration}-{mode}';directory.mkdir()
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
        result=check_result(mode,code,timeout,text)
        result.update(iteration=iteration,evidence=str(directory.relative_to(evidence)))
        results.append(result)
        # Preserve partial results after each case, even if a later case crashes.
        for target in (session/'results.json',evidence/'results.json'):
            target.write_text(json.dumps(results,indent=2)+'\n',encoding='utf-8')
        print(json.dumps(result),flush=True)
    return 0 if all(r['passed'] for r in results) else 1


if __name__=='__main__':
    sys.exit(main())
