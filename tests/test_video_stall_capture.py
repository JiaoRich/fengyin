from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]
CAPTURE = ROOT / 'scripts/video-stall-capture'


class VideoStallCapture(unittest.TestCase):
    def test_external_read_only_window_state(self):
        source = (CAPTURE / 'WindowSnapshot.cs').read_text(encoding='utf-8')
        for required in ('GetWindowInfo', 'GetGUIThreadInfo', 'WindowFromPoint',
                         'GetClassName', 'IsWindowEnabled', 'IsWindowVisible',
                         'EnumChildWindows', 'TargetPID', 'Truncated'):
            self.assertIn(required, source)
        for forbidden in ('SetFocus(', 'SetForegroundWindow(', 'SendInput(', 'SendMessage(',
                          'SuspendThread(', 'ReadProcessMemory(', 'GetWindowText(', 'SetWindowPos('):
            self.assertNotIn(forbidden, source)

    def test_bounded_session_and_no_mutation(self):
        source = (CAPTURE / 'Capture.ps1').read_text(encoding='utf-8')
        self.assertIn('[ValidateRange(30,300)]', source)
        self.assertIn("Run-Wpr '-start CPU'", source)
        self.assertNotIn('-filemode', source)
        self.assertNotIn('-compress', source)
        self.assertNotIn('-details', source)
        self.assertIn("$Arguments+' -instancename '+$Instance", source)
        self.assertIn("Run-Wpr '-cancel' 'wpr-cancel-own'", source)
        self.assertIn('if($Tracing)', source)
        self.assertIn("$Tracing=$true", source)
        for forbidden in ('Stop-Process', 'Set-ItemProperty', 'SetEnvironmentVariable',
                          'SetPriority', 'powercfg', 'FENGYIN_PERFORMANCE_NOTES'):
            self.assertNotIn(forbidden, source)
        self.assertIn('-Encoding UTF8 -Tail', source)
        self.assertIn('PrivilegedProcessorTime', source)
        self.assertIn('Name=\'msedgewebview2.exe\'', source)
        self.assertIn('ParentProcessId', source)
        self.assertNotIn('CommandLine', source)
        self.assertLess(source.index("'session.json'"), source.index("Run-Wpr '-start CPU'"))
        wrapper = (CAPTURE / 'NativeCommands.ps1').read_text(encoding='utf-8')
        self.assertIn('New-Object System.Diagnostics.Process', wrapper)
        self.assertIn('$null -eq $code', wrapper)
        self.assertIn('ReadToEndAsync()', wrapper)
        self.assertIn('FengYinVideo-[0-9a-fA-F]{32}', wrapper)
        self.assertNotIn('Start-Process -FilePath', wrapper)

    def test_actual_windows_smoke_is_required(self):
        workflow = (ROOT / '.github/workflows/windows-build.yml').read_text(encoding='utf-8')
        self.assertIn('windows-2022', workflow)
        self.assertIn('./scripts/video-stall-capture/Test.ps1', workflow)
        self.assertIn('if: success()', workflow)
        smoke = (CAPTURE / 'Test.ps1').read_text(encoding='utf-8')
        for required in ('$panel.Enabled=$false', '$panel.Enabled=$true', '$form.Hide()',
                         '-start CPU -instancename', '-status -instancename',
                         '-skipPdbGen -instancename', 'ETL ZIP round trip changed bytes'):
            self.assertIn(required, smoke)

        for required in ("-File (Join-Path $copyRoot 'Capture.ps1') -NoPrompt -Seconds 30",
                         'collector script (4)', 'exact entry output (4)',
                         'Exact entry missing evidence', 'User entry leaked an ETW session',
                         'collector-exact-entry.zip'):
            self.assertIn(required, smoke)


if __name__ == '__main__':
    unittest.main()
