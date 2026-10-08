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

    def test_sound_check_opaque_and_themed(self):
        source=(ROOT/'prototype/styles.css').read_text(encoding='utf-8')
        self.assertIn('.sound-check-dialog::backdrop',source)
        self.assertIn('background:var(--bg,#07101d)',source)
        self.assertIn('.sound-check-dialog button',source)
        html=(ROOT/'prototype/index.html').read_text(encoding='utf-8')
        self.assertNotIn('<strong>低延迟组件</strong>',html)

    def test_normal_start_does_not_require_private_api(self):
        source=(ROOT/'audio-engine/windows/AudioEngineMain.cpp').read_text(encoding='utf-8')
        self.assertIn('bool requireEndpointMatch = false',source)
        self.assertIn('error, requireEndpointMatch)',source)
        source=(ROOT/'audio-engine/windows/Asio4AllOutput.cpp').read_text(encoding='utf-8')
        self.assertIn('selectedEndpoint = requireMatch',source)

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

    def test_sound_check_cancel_and_failure(self):
        source=(ROOT/'prototype/app.js').read_text(encoding='utf-8')
        self.assertIn("soundCheckDialog.addEventListener('cancel'",source)
        self.assertIn("disabled=!String(message).startsWith('正在播放')",source)
        self.assertIn('soundCheckIndex >= soundCheckChoices.length',source)

    def test_manual_output_not_overwritten(self):
        source=(ROOT/'audio-engine/windows/AudioEngineMain.cpp').read_text(encoding='utf-8')
        self.assertIn('!manualPhysicalOutput',source)

if __name__=='__main__': unittest.main()
