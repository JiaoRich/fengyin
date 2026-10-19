from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]


class VideoFastEntryTests(unittest.TestCase):
    def test_builds_only_existing_production_source_diagnostic(self):
        source = (ROOT / 'scripts/test-video-fast.ps1').read_text(encoding='utf-8')
        self.assertIn('--target FengYinVlcWebViewCheck', source)
        self.assertIn('check_vlc_webview.py', source)
        self.assertIn('if ($LASTEXITCODE -ne 0)', source)
        self.assertIn('if ($LASTEXITCODE -ge 8)', source)
        self.assertNotIn('Remove-Item', source)
        self.assertNotIn('/MIR', source)
        self.assertNotIn('build-windows.ps1', source)

    def test_reuse_dependencies_and_do_not_overclaim(self):
        source = (ROOT / 'scripts/test-video-fast.ps1').read_text(encoding='utf-8')
        self.assertIn('FETCHCONTENT_SOURCE_DIR_JUCE', source)
        self.assertIn('fengyin-source.json', source)
        self.assertIn('Build only; no tests passed', source)
        self.assertIn('Real instrument, speaker and recording acceptance remains required', source)


if __name__ == '__main__':
    unittest.main()
