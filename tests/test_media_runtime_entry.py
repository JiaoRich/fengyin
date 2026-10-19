from pathlib import Path
import unittest

ROOT=Path(__file__).resolve().parents[1]


class MediaRuntimeEntryTests(unittest.TestCase):
    def test_runtime_uses_production_targets_without_installer(self):
        build=(ROOT/'scripts/build-media-runtime.ps1').read_text(encoding='utf-8')
        self.assertIn('--target FengYin FengYinRecorder FengYinAudioEngine FengYinAudioWatchdog FengYinDriverSetup',build)
        self.assertIn("'-DFENGYIN_ARCHIVED_VIDEO_RECORDING=OFF'",build)
        self.assertIn('Production JUCE patches',build)
        self.assertIn('verify_system_recording.py',build)
        self.assertIn('Get-FileHash',build)
        self.assertIn('Full Windows CTest, target instrument/video/recording acceptance',build)
        self.assertNotIn('Remove-Item',build)
        self.assertNotIn('innosetup',build)
        workflow=(ROOT/'.github/workflows/windows-build.yml').read_text(encoding='utf-8')
        self.assertIn('!inputs.media_runtime_test && !inputs.vlc_webview_test',workflow)
        self.assertIn('Windows media performance runtime (no installer)',workflow)

    def test_launcher_is_process_local_and_never_changes_performance_settings(self):
        run=(ROOT/'scripts/media-runtime-test/Run.ps1').read_text(encoding='utf-8')
        self.assertIn("$env:FENGYIN_AUDIO_TIMING = '1'",run)
        self.assertIn("$env:FENGYIN_PERFORMANCE_NOTES = '0'",run)
        self.assertIn("SetEnvironmentVariable($Name,$Saved[$Name],'Process')",run)
        self.assertIn('SaveLogs \'before\'',run)
        self.assertIn('SaveLogs \'after\'',run)
        for forbidden in ('Stop-Process','powercfg','reg add','setx','SendKeys','Set-Content -LiteralPath $Exe'):
            self.assertNotIn(forbidden,run)


if __name__=='__main__':
    unittest.main()
