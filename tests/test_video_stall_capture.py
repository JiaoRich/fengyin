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
        self.assertIn("$Arguments+' -instancename '+$Session", source)
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

    def test_actual_windows_smoke_is_required(self):
        workflow = (ROOT / '.github/workflows/windows-build.yml').read_text(encoding='utf-8')
        self.assertIn('windows-2022', workflow)
        self.assertIn('./scripts/video-stall-capture/Test.ps1', workflow)
        self.assertIn('if: success()', workflow)
        smoke = (CAPTURE / 'Test.ps1').read_text(encoding='utf-8')
        for required in ('$panel.Enabled=$false', '$panel.Enabled=$true', '$form.Hide()',
                         '-start CPU -instancename', '-status collectors -details -instancename',
                         '-skipPdbGen -compress -instancename', 'ETL ZIP round trip changed bytes'):
            self.assertIn(required, smoke)


if __name__ == '__main__':
    unittest.main()
