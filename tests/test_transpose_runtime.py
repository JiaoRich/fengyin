import subprocess
import unittest
import shutil
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
NODE = shutil.which('node')
if NODE is None:
    bundled = Path('/Users/mengyang.jmy/.cache/codex-runtimes/codex-primary-runtime/dependencies/node/bin/node')
    if bundled.is_file():
        NODE = str(bundled)

class TransposeRuntimeTests(unittest.TestCase):
    def test_buttons_labels_limits_and_announcements(self):
        self.assertIsNotNone(NODE, 'Node.js is required to execute the production transpose test')
        source = (ROOT/'prototype/app.js').read_text(encoding="utf-8")
        start = source.index('function updateTransposeDisplay(')
        end = source.index("$('#transpose-minus').addEventListener", start)
        harness = """
const assert = require('assert');
let selectedTargetKey=0;
const nodes={}; const $=id=>nodes[id]||(nodes[id]={});
let spoken='',event;
const window={fengyinVoice:{speakText:t=>spoken=t}};
const nativeEvent=(n,p)=>event={n,p};
""" + source[start:end] + """
updateTransposeDisplay(0); assert.equal(nodes['#transpose-key-name'].textContent,'移调0');
changeTranspose(1); assert.equal(spoken,'加一个半音'); assert.equal(event.p.targetKey,1);
changeTranspose(-1); assert.equal(spoken,'关闭移调');
changeTranspose(-1); assert.equal(spoken,'减一个半音');
updateTransposeDisplay(12); assert.equal(nodes['#transpose-plus'].disabled,true);
assert.equal(nodes['#transpose-key-name'].textContent,'移调+12');
updateTransposeDisplay(-12); assert.equal(nodes['#transpose-minus'].disabled,true);
updateTransposeDisplay(99); assert.equal(selectedTargetKey,12);
"""
        result = subprocess.run([str(NODE),'-e',harness],capture_output=True,text=True,encoding='utf-8')
        self.assertEqual(result.returncode,0,result.stderr)
