from pathlib import Path
import json
import shutil
import subprocess
import tempfile
import unittest

ROOT=Path(__file__).resolve().parents[1]

class Release143(unittest.TestCase):
    def test_actual_curve_properties(self):
        compiler=shutil.which('clang++') or shutil.which('g++')
        self.assertIsNotNone(compiler)
        with tempfile.TemporaryDirectory() as folder:
            exe=str(Path(folder)/'curve')
            subprocess.run([compiler,'-std=c++20','-O2','-UNDEBUG','-I',str(ROOT/'native/Source'),
                            str(ROOT/'tests/test_breath_sensitivity.cpp'),'-o',exe],check=True,capture_output=True)
            subprocess.run([exe],check=True,capture_output=True)

    def test_device_response_is_not_preset_or_technique_mutation(self):
        midi=(ROOT/'native/Source/MidiInputService.cpp').read_text(encoding='utf-8')
        self.assertIn('controllerSettingKey() + ".sensitivityV1"',midi)
        self.assertIn('BreathSensitivity::apply(currentBreath, getBreathSensitivity())',midi)
        self.assertIn('BreathSensitivity::apply(rawBreath, getBreathSensitivity())',midi)
        self.assertIn('updateBreathDrivenTechniques(rawBreath, sink',midi)
        main=(ROOT/'native/Source/MainComponent.cpp').read_text(encoding='utf-8')
        bridge=main.split('.withEventListener("setBreathSensitivity"',1)[1].split('.withEventListener(',1)[0]
        self.assertIn('deviceIdentifier',bridge);self.assertIn('connectionGeneration',bridge)
        self.assertNotIn('masterOutput',bridge);self.assertNotIn('pluginHost',bridge)
        self.assertIn('if (commit) lastWebStateAtMs = 0;',bridge)

    def test_runtime_is_pinned_and_verified(self):
        manifest=json.loads((ROOT/'installer/vc-runtime.json').read_text(encoding='utf-8'))
        self.assertTrue(manifest['url'].startswith('https://download.visualstudio.microsoft.com/'))
        self.assertEqual(len(manifest['sha256']),64)
        script=(ROOT/'installer/Install-VCRuntime.ps1').read_text(encoding='utf-8')
        for gate in ['Get-AuthenticodeSignature','Get-FileHash','binaryVersion','Read-InstalledVCRuntime','Test-VCRuntimeFiles','LoadLibraryEx','/norestart']:
            self.assertIn(gate,script)
        self.assertNotIn('/uninstall',script)
        installer=(ROOT/'installer/FengYin.iss').read_text(encoding='utf-8')
        procedure=installer.split('procedure CurStepChanged',1)[1]
        self.assertLess(procedure.index('PrepareVCRuntime();'),procedure.index('RunRequiredComponent('))

    def test_ui_has_single_device_scoped_control_and_footer(self):
        html=(ROOT/'prototype/index.html').read_text(encoding='utf-8')
        self.assertEqual(html.count('id="breath-sensitivity"'),1)
        self.assertIn('class="advanced-footer"',html)
        js=(ROOT/'prototype/app.js').read_text(encoding='utf-8')
        self.assertIn("connectionGeneration:state.connectionGeneration",js)
        self.assertIn("if(sensitivityPointer)for(const anchor",js)
        self.assertIn("'breath','rawBreath','instrumentPeak'",js)

    def test_tone_and_voice_assets_unchanged(self):
        changed=subprocess.check_output(['git','diff','--name-only','HEAD','--','assets','native/Source/PublishedToneCatalog.h'],cwd=ROOT,text=True)
        self.assertEqual(changed,'','This release must not alter the tone/speech payloads or catalogue')
