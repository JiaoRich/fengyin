from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]


class Feedback117Tests(unittest.TestCase):
    def test_plugin_editor_is_not_topmost_and_reopen_clamps_title(self):
        source = (ROOT / 'native/Source/PluginHostEngine.cpp').read_text(encoding='utf-8')
        editor = source.split('class PluginHostEngine::PluginEditorWindow', 1)[1].split('\n};', 1)[0]
        self.assertIn('setAlwaysOnTop(false)', editor)
        self.assertNotIn('setAlwaysOnTop(true)', editor)
        self.assertIn('display->userArea', editor)
        reopen = source.split('bool PluginHostEngine::showPluginEditor', 1)[1].split('\nvoid ', 1)[0]
        self.assertIn('window->keepTitleBarOnScreen()', reopen)

    def test_card_delete_uses_id_and_does_not_retune_live_sound(self):
        js = (ROOT / 'prototype/app.js').read_text(encoding='utf-8')
        self.assertIn("nativeEvent('deletePreset',{id:cards[i].presetId})", js)
        source = (ROOT / 'native/Source/MainComponent.cpp').read_text(encoding='utf-8')
        delete = source.split('void MainComponent::deleteSelectedPreset()', 1)[1].split('\nvoid ', 1)[0]
        for write in ('applySwamToneProfile(', 'setToneStyle(', 'applyExpressionAttackControl('):
            self.assertNotIn(write, delete)
        self.assertIn('删除失败', delete)

    def test_card_identity_is_catalogue_specific_and_sizes_are_equal(self):
        js = (ROOT / 'prototype/app.js').read_text(encoding='utf-8')
        self.assertIn('id:`builtin:${tone.id}`', js)
        self.assertIn('Math.min(5,', js)
        html = (ROOT / 'prototype/index.html').read_text(encoding='utf-8')
        self.assertIn('grid-auto-rows:96px', html)
        self.assertIn('.action-bar #music-tempo>span{display:inline', html)


if __name__ == '__main__':
    unittest.main()
