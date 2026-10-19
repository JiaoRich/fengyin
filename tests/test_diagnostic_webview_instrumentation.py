import hashlib
import importlib.util
from pathlib import Path
import tempfile
import unittest

ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('diag_webview',ROOT/'scripts/prepare_diagnostic_webview.py')
patch=importlib.util.module_from_spec(spec);spec.loader.exec_module(patch)

class DiagnosticWebViewInstrumentation(unittest.TestCase):
    def test_patch_is_idempotent_and_rejects_unknown_source(self):
        source=patch.NEEDLE+'\n';expected=hashlib.sha256(source.encode()).hexdigest()
        with tempfile.TemporaryDirectory() as directory:
            file=Path(directory)/'webview.cpp';file.write_text(source,encoding='utf-8')
            first=patch.apply(file,expected);self.assertEqual(patch.apply(file,expected),first)
            self.assertEqual(file.read_text(encoding='utf-8').count(patch.PATCH),1)
            file.write_text('unknown source',encoding='utf-8')
            with self.assertRaises(ValueError):patch.apply(file,expected)

    def test_release_does_not_patch_juce_or_use_browser_flags(self):
        cmake=(ROOT/'tools/vlc-webview-check/CMakeLists.txt').read_text(encoding='utf-8')
        self.assertIn('if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)',cmake)
        self.assertIn('COMMAND_ERROR_IS_FATAL ANY',cmake)
        main=(ROOT/'native/Source/MainComponent.cpp').read_text(encoding='utf-8')
        self.assertNotIn('FENGYIN_DIAGNOSTIC_WEBVIEW_ARGS',main)
        workflow=(ROOT/'.github/workflows/windows-build.yml').read_text(encoding='utf-8')
        self.assertIn("steps.default-interaction.outcome == 'failure'",workflow)
        self.assertIn('FENGYIN_DIAGNOSTIC_WEBVIEW_ARGS',workflow)
