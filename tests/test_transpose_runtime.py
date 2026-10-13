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
        end = source.index("$('#performance-key').addEventListener", start)
        harness = """
const assert = require('assert');
let selectedTargetKey=0;
const nodes={}; const $=id=>nodes[id]||(nodes[id]={});
let spoken='',event;
const window={fengyinVoice:{say:t=>spoken=t}};
const nativeEvent=(n,p)=>event={n,p};
""" + source[start:end] + """
for(let key=0;key<12;key++){
 changePerformanceKey(key); assert.equal(nodes['#performance-key'].value,String(key));
 assert.equal(event.p.targetKey,key); assert.equal(spoken,'V026_'+String(key+1).padStart(2,'0'));
}
updateTransposeDisplay(12);assert.equal(selectedTargetKey,0);
updateTransposeDisplay(-2);assert.equal(selectedTargetKey,10);
"""
        result = subprocess.run([str(NODE),'-e',harness],capture_output=True,text=True,encoding='utf-8')
        self.assertEqual(result.returncode,0,result.stderr)
