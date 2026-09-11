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
        script_ids = set(re.findall(r"\$\('#([^']+)'\)", JS))
        self.assertEqual(set(), script_ids - html_ids)

    def test_required_first_milestone_controls_exist(self):
        for control_id in (
            "video-file", "play-button", "spectrum", "breath-bar",
            "meter-l", "meter-r", "theme", "license-code", "activate"
        ):
            self.assertIn(f'id="{control_id}"', HTML)

    def test_only_one_page_starts_active(self):
        active_pages = re.findall(r'<section class="page active"', HTML)
        self.assertEqual(1, len(active_pages))


if __name__ == "__main__":
    unittest.main()
