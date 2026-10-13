from pathlib import Path
import unittest,json,zipfile,xml.etree.ElementTree as ET,hashlib
ROOT=Path(__file__).resolve().parents[1]
class Release113(unittest.TestCase):
    def read(self,p): return (ROOT/p).read_text(encoding='utf-8')
    def test_optional_bridge_and_default_shared(self):
        installer=self.read('installer/FengYin.iss')
        self.assertIn("if WizardIsTaskSelected('asiobridge') then",installer)
        self.assertIn('Flags: unchecked',installer.split('Name: "asiobridge"',1)[1].splitlines()[0])
        service=self.read('native/Source/AudioDeviceService.cpp')
        self.assertIn('getBoolValue("audioEngineEnabled", false)',service)
        self.assertIn('getAvailableBufferSizes().contains(128)',service)
        self.assertIn('bridgeDependenciesAvailable()',service)
    def test_plugin_state_and_voice_switch(self):
        source=self.read('native/Source/PluginHostEngine.cpp')
        self.assertIn('KongProjectFile::readPluginComponentState(state)',source)
        self.assertIn('eventLock(performanceEventLock)',source)
        self.assertIn('suppressTechniqueMidi',source)
        self.assertIn('messages.swapWith(filteredMidi)',source)
        self.assertIn('activeVelocities[static_cast<size_t>(note)]',source)
        self.assertIn('Vib_Mellow',source)
        policy=self.read('native/Source/KongPerformancePolicy.h')
        self.assertLess(policy.index('noteOff(channel, note)'),policy.index('noteOn(channel, key'))
        self.assertLess(policy.index('noteOff(channel, key)'),policy.index('noteOn(channel, note, velocity'))
    def test_off_gate_not_curve_zero_and_kontakt(self):
        source=self.read('native/Source/PluginHostEngine.cpp')
        gate=source.split('if (safeValue == 0.0f',1)[1].split('// A direct depth',1)[0]
        self.assertNotIn('queue(',gate)
        js=self.read('prototype/app.js')
        self.assertIn("$('#configure-asio4all').hidden = !asioBridge",js)
        self.assertIn("group.label = '采样音源 · Kontakt'",js)
        self.assertIn('option.value = String(index)',js)
    def test_original_tone_rename_and_manifest(self):
        with zipfile.ZipFile(ROOT/'assets/tone-packages/soprano-kenny.fytonepack') as archive:
            tone=archive.read('tone.xml');manifest=json.loads(archive.read('manifest.json'))
        self.assertEqual(ET.fromstring(tone).find('PRESET').get('name'),'高萨-原声')
        self.assertEqual(manifest['style'],'高萨-原声')
        entry=next(i for i in manifest['files'] if i['path']=='tone.xml')
        self.assertEqual(entry['sha256'],hashlib.sha256(tone).hexdigest())
        self.assertEqual(entry['bytes'],len(tone))
        catalog=json.loads(self.read('prototype/voice-catalog.js').split('=',1)[1].strip().rstrip(';'))
        self.assertEqual(catalog['voice'],'Tingting')
        self.assertEqual(next(i for i in catalog['items'] if i['id']=='V014_01')['text'],'高音萨克斯，原声。')
