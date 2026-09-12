import pathlib
import re
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[1]
HTML = (ROOT / "prototype" / "index.html").read_text(encoding="utf-8")
JS = (ROOT / "prototype" / "app.js").read_text(encoding="utf-8")


class PrototypeStructureTests(unittest.TestCase):
    def test_all_primary_pages_exist(self):
        for page in ("play", "sounds", "chain", "wind", "audio", "settings"):
            self.assertIn(f'id="page-{page}"', HTML)
            self.assertIn(f'data-page="{page}"', HTML)

    def test_script_references_existing_ids(self):
        html_ids = set(re.findall(r'id="([^"]+)"', HTML))
        # Ignore the second dollar sign in the $$() query-all helper.
        script_ids = set(re.findall(r"(?<!\$)\$\('#([^']+)'\)", JS))
        self.assertEqual(set(), script_ids - html_ids)

    def test_required_first_milestone_controls_exist(self):
        for control_id in (
            "video-file", "play-button", "spectrum", "breath-bar",
            "meter-l", "meter-r", "theme", "license-code", "activate", "scan-swam"
        ):
            self.assertIn(f'id="{control_id}"', HTML)

    def test_only_one_page_starts_active(self):
        active_pages = re.findall(r'<section class="page active"', HTML)
        self.assertEqual(1, len(active_pages))

    def test_video_errors_use_non_blocking_toast(self):
        self.assertIn("video.addEventListener('error'", JS)
        self.assertNotIn("alert(", JS)

    def test_confirmed_visual_features_are_present(self):
        for element_id in ("theme-atmosphere", "breath-wave", "layout-resizer", "lower-stage", "instrument-picture"):
            self.assertIn(f'id="{element_id}"', HTML)
        for theme in ("spring", "summer", "autumn", "winter", "china-red", "gold", "neon", "minimal"):
            self.assertIn(f'value="{theme}"', HTML)
        self.assertIn("drawThemeAtmosphere", JS)

    def test_instrument_artwork_covers_every_swam_family(self):
        for key in (
            "soprano-sax", "alto-sax", "tenor-sax", "baritone-sax",
            "piccolo-trumpet", "double-bass-trombone", "euphonium", "horn-bb",
            "piccolo", "bass-flute", "bass-clarinet", "english-horn", "contrabassoon",
            "violin", "viola", "cello", "double-bass",
        ):
            self.assertIn(f"'{key}'", JS)
        self.assertIn("showInstrumentArtwork", JS)
        self.assertIn("lowerMinimum", JS)

    def test_live_controls_have_real_behaviour(self):
        self.assertIn('id="recording-manager"', HTML)
        self.assertIn("nativeEvent('showRecordings')", JS)
        self.assertIn('id="wind-status-text">未连接电吹管', HTML)
        self.assertIn('id="favorite-current"', HTML)
        self.assertIn('id="favorite-instruments"', HTML)
        self.assertIn("fengyin-favorite-instruments-v1", JS)
        self.assertIn("slice(0, 3)", JS)
        self.assertIn("document.body.classList.add('video-playing')", JS)
        self.assertIn("document.body.classList.remove('video-playing')", JS)

    def test_instrument_art_uses_independent_images_without_distortion(self):
        self.assertIn('object-fit:contain', HTML)
        self.assertIn('assets/instruments/instrument_soprano_sax.png', HTML)
        self.assertNotIn('instrument-saxophones.png', JS)
        for filename in ("instrument_soprano_sax.png", "instrument_trumpet.png",
                         "instrument_flute.png", "instrument_violin.png"):
            self.assertTrue((ROOT / "assets" / "instruments" / filename).is_file())

    def test_preset_navigation_waits_for_success(self):
        self.assertIn("presetLoadResult", JS)
        self.assertIn("pluginLoadResult", JS)
        self.assertIn("if (!result.success)", JS)

    def test_only_one_lower_stage_container_exists(self):
        self.assertEqual(1, HTML.count('id="lower-stage"'))

    def test_html_container_tags_are_balanced(self):
        self.assertEqual(len(re.findall(r'<div(?:\s|>)', HTML)), HTML.count('</div>'))

    def test_plugin_manager_rows_have_explicit_layout(self):
        self.assertIn('.plugin-manager>#instrument-load-state{grid-area:2/2/3/4}', HTML)
        self.assertIn('.plugin-manager>label[for="effect-select"]{grid-area:3/1}', HTML)
        self.assertIn('.plugin-manager>#effect-load-state{grid-area:4/2/5/4}', HTML)
        self.assertEqual(HTML.count('{'), HTML.count('}'))


if __name__ == "__main__":
    unittest.main()
