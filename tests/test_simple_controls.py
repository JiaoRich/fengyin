import json
import shutil
import subprocess
import unittest
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
JS=(ROOT/'prototype/app.js').read_text(encoding='utf-8')
MIDI=(ROOT/'native/Source/MidiInputService.cpp').read_text(encoding='utf-8')

class SimpleControlsTests(unittest.TestCase):
    def test_three_roles_and_saved_modes_execute_production_js(self):
        node=shutil.which('node')
        fallback=Path('/Users/mengyang.jmy/.cache/codex-runtimes/codex-primary-runtime/dependencies/node/bin/node')
        if not node and fallback.is_file():node=str(fallback)
        self.assertIsNotNone(node)
        plan=JS[JS.index('function buildTechniquePlan('):JS.index('function buildAdaptiveTechniquePlan(')]
        merge=JS[JS.index('function mergeBackendTechniquePlan('):JS.index('function inferInstrumentKey(')]
        harness="const assert=require('assert');const window={};let currentInstrument=null;\n"+plan+merge+"""
for(const key of ['soprano-sax','alto-sax','tenor-sax','baritone-sax','violin-section','flute','kong-erhu','kong-pipa'])assert.equal(buildTechniquePlan(key).length,3);
assert(!buildTechniquePlan('soprano-sax')[1].pluginSupported);
assert(buildTechniquePlan('alto-sax')[1].pluginSupported);
assert(buildTechniquePlan('violin-section')[0].pluginSupported);
assert(!buildTechniquePlan('flute')[0].pluginSupported);
assert.equal(buildTechniquePlan('kong-pipa')[2].name,'其他技巧：轮指');
let p=mergeBackendTechniquePlan('alto-sax',{},[{id:'vibrato',mode:4,sourceType:1,sourceNumber:74},{id:'growl',mode:1,pluginSupported:false}]);
assert.equal(p[0].mode,'off');assert.equal(p[0].sourceNumber,74);assert(p[1].routeMissing);
"""
        run=subprocess.run([node,'-e',harness],capture_output=True,text=True,encoding='utf-8')
        self.assertEqual(run.returncode,0,run.stderr)

    def test_visual_scheduler_has_no_recurring_draw_loop(self):
        draw=JS[JS.index('function draw(timestamp'):JS.index('window.addEventListener(\'focus\'')]
        self.assertNotIn('drawThemeAtmosphere(ratio',draw)
        self.assertNotIn('animationFrame = requestAnimationFrame(draw)',draw)
        self.assertIn('1000/24',draw)
        self.assertIn('document.hidden',draw)
        self.assertIn('updateStaticTheme()',JS)
        self.assertNotIn('1000/60',draw)

    def test_pitch_reserved_and_no_second_curve(self):
        handle=MIDI[MIDI.index('bool MidiInputService::handleTechniqueMessage'):MIDI.index('void MidiInputService::updateBreathDrivenTechniques')]
        self.assertIn('if (message.isPitchWheel()) return false',handle)
        self.assertIn('mapping.mode != TechniqueControlMode::hardware',handle)
        self.assertIn('PitchKey::transposeFromTo(0,',MIDI)
        self.assertIn('performanceKeyV3',MIDI)
        self.assertNotIn('getIntValue("transposeOffsetV2"',MIDI)
        self.assertIn('b * automaticVibratoFade(elapsed)',MIDI)
