from pathlib import Path
import ast
import unittest

ROOT = Path(__file__).resolve().parents[1]

class Release112(unittest.TestCase):
    def test_mode_switch_preserves_browser_peer(self):
        source=(ROOT/'native/Source/Main.cpp').read_text(encoding='utf-8')
        mode=source.split('void applyMode(bool compact)',1)[1].split('juce::BorderSize<int>',1)[0]
        self.assertNotIn('setUsingNativeTitleBar',mode)
        self.assertNotIn('setResizable',mode)

    def test_sound_check_removed(self):
        for file in ['prototype/styles.css', 'prototype/app.js', 'prototype/index.html',
                     'native/Source/MainComponent.cpp', 'native/Source/AudioDeviceService.cpp',
                     'native/Source/AudioDeviceService.h']:
            source=(ROOT/file).read_text(encoding='utf-8')
            self.assertNotIn('sound-check',source)
            self.assertNotIn('SoundCheck',source)
            self.assertNotIn('soundCheck',source)
        html=(ROOT/'prototype/index.html').read_text(encoding='utf-8')
        self.assertNotIn('<strong>低延迟组件</strong>',html)

    def test_normal_start_does_not_require_private_api(self):
        source=(ROOT/'audio-engine/windows/AudioEngineMain.cpp').read_text(encoding='utf-8')
        self.assertIn('bool requireEndpointMatch = false',source)
        self.assertIn('error, requireEndpointMatch)',source)
        source=(ROOT/'audio-engine/windows/Asio4AllOutput.cpp').read_text(encoding='utf-8')
        self.assertIn('selectedEndpoint = requireMatch && !preferredEndpoint.empty()',source)
        self.assertIn('if (requireMatch && !selectedEndpoint)', source)

    def test_source_readers_have_explicit_encoding(self):
        for path in (ROOT/'tests').glob('test_*.py'):
            tree = ast.parse(path.read_text(encoding='utf-8'))
            for node in ast.walk(tree):
                if (isinstance(node, ast.Call) and isinstance(node.func, ast.Attribute)
                        and node.func.attr == 'read_text'):
                    self.assertTrue(node.args or any(k.arg == 'encoding' for k in node.keywords),
                                    f'{path.name}:{node.lineno}: read_text requires encoding')

    def test_borderless_work_area(self):
        source=(ROOT/'native/Source/Main.cpp').read_text(encoding='utf-8')
        for text in ['setUsingNativeTitleBar(false)', 'setTitleBarHeight(0)',
                     'area.reduced(12)', 'juce::BorderSize<int>(0)']:
            self.assertIn(text,source)

    def test_exact_endpoint_before_mutation(self):
        source=(ROOT/'audio-engine/windows/AsioEndpointSelection.h').read_text(encoding='utf-8')
        self.assertLess(source.index('if (chosenD < 0'),source.index('auto change ='))
        self.assertIn('if (c.attempted) return FALSE',source)
        self.assertIn('api->callback(nullptr,nullptr)',source)
        self.assertIn('c.changed.rbegin()',source)

    def test_hotplug_reopens_even_when_default_is_unchanged(self):
        source=(ROOT/'audio-engine/windows/AudioEngineMain.cpp').read_text(encoding='utf-8')
        self.assertNotIn('!manualPhysicalOutput',source)
        self.assertNotIn('newPhysicalEndpoint != preferredEndpointId',source)
        router=(ROOT/'audio-engine/windows/DefaultEndpointRouter.cpp').read_text(encoding='utf-8')
        self.assertIn('stableEndpointPolls < 3',router)
        self.assertIn('now + 500',router)
        self.assertIn('readPhysicalEndpoints',router)

    def test_fresh_air_bundle_and_retry(self):
        source=(ROOT/'installer/FengYin.iss').read_text(encoding='utf-8')
        self.assertIn(r'\Contents\x86_64-win\Fresh Air.vst3',source)
        verify=source.split('procedure VerifyFreshAirInstalled();',1)[1].split('function NeedRestart',1)[0]
        self.assertNotIn('RaiseException',verify)
        self.assertIn('install-retry.log',verify)
        self.assertIn('ewWaitUntilTerminated',verify)

    def test_asio_output_has_action_instead_of_placeholder(self):
        html=(ROOT/'prototype/index.html').read_text(encoding='utf-8')
        source=(ROOT/'prototype/app.js').read_text(encoding='utf-8')
        self.assertEqual(html.count('id="configure-asio4all"'),1)
        self.assertNotIn('高级输出设备设置',html)
        self.assertIn("$('#audio-output-select').hidden = asioBridge",source)
        self.assertIn("$('#configure-asio4all').hidden = !asioBridge",source)
        self.assertIn("$('#configure-asio4all').disabled = busy",source)

if __name__=='__main__': unittest.main()
