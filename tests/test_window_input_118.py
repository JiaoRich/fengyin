from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]


class WindowInput118Tests(unittest.TestCase):
    def test_page_and_native_zoom_are_both_disabled(self):
        html = (ROOT / 'prototype/index.html').read_text(encoding='utf-8')
        css = (ROOT / 'prototype/styles.css').read_text(encoding='utf-8')
        patch = (ROOT / 'scripts/patch-juce-wasapi-raw.cmake').read_text(encoding='utf-8')
        self.assertIn('maximum-scale=1.0, user-scalable=no', html)
        self.assertIn('touch-action:pan-x pan-y', css)
        self.assertIn('put_IsPinchZoomEnabled (false)', patch)
        self.assertIn('put_IsZoomControlEnabled (false)', patch)
        self.assertIn('FENGYIN_LOCK_WEBVIEW_ZOOM=1', (ROOT / 'CMakeLists.txt').read_text(encoding='utf-8'))

    def test_drag_is_single_pointer_and_cleans_up(self):
        js = (ROOT / 'prototype/app.js').read_text(encoding='utf-8')
        begin = js.split('function beginTouchWindowGesture', 1)[1].split("const windowControls", 1)[0]
        self.assertIn('e.isPrimary === false || touchWindowGesture', begin)
        self.assertNotIn("e.pointerType !== 'touch'", begin)
        self.assertIn("window.addEventListener('blur',cancelWindowGesture)", begin)
        self.assertIn('releasePointerCapture', begin)
        self.assertIn('1000/24', begin)

    def test_bend_picker_is_five_choices_not_a_modal(self):
        html = (ROOT / 'prototype/index.html').read_text(encoding='utf-8')
        picker = html.split('id="bend-range-dialog"', 1)[1].split('id="transpose-dialog"', 1)[0]
        self.assertIn('role="menu"', picker)
        self.assertNotIn('role="dialog"', picker)
        self.assertNotIn('aria-modal', picker)
        for value in range(1, 5):
            self.assertIn(f'data-bend="{value}"', picker)
        self.assertIn('恢复推荐</button>', picker)

    def test_string_identity_is_content_not_pointer_comparison(self):
        source = (ROOT / 'native/Source/PluginHostEngine.cpp').read_text(encoding='utf-8')
        self.assertEqual(source.count('const std::string_view key=SwamPluginClassifier::instrumentKey'), 2)


if __name__ == '__main__':
    unittest.main()
