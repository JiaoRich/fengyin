from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]


class AudioStartupRecovery(unittest.TestCase):
    def test_both_driver_installers_require_restart(self):
        source = (ROOT / 'installer/FengYin.iss').read_text(encoding='utf-8')
        asio = next(line for line in source.splitlines()
                    if line.startswith('Filename:') and 'AfterInstall: MarkAudioDriverRestart' in line)
        self.assertIn('ASIO4ALL_2_22.exe', asio)
        self.assertIn('VBCableInstalledThisRun or AudioDriverRestartRequired', source)
        self.assertIn('not NeedRestart()', source)
        self.assertIn('FinishedRestartMessage=', source)

    def test_windows_startup_never_uses_unchecked_system_default(self):
        source = (ROOT / 'native/Source/AudioDeviceService.cpp').read_text(encoding='utf-8')
        startup = source.split('AudioDeviceService::initialise()', 1)[1].split(
            'AudioDeviceService::restorePhysicalSharedOutput()', 1)[0]
        self.assertIn('restorePhysicalSharedOutput()', startup)
        self.assertIn('manager.initialise(0, 2, saved.get(), false)', startup)
        self.assertIn('isVirtualAudioOutput(saved->getStringAttribute', startup)
        self.assertIn('低延迟模式尚未恢复', startup)
        self.assertNotIn('manager.initialise(0, 2, nullptr, true)', startup)

    def test_shared_outputs_filter_virtual_routes(self):
        source = (ROOT / 'native/Source/AudioDeviceService.cpp').read_text(encoding='utf-8')
        outputs = source.split('AudioDeviceService::getAvailableOutputDevices', 1)[1].split(
            'AudioDeviceService::isSharedAsioModeAvailable', 1)[0]
        self.assertIn('isVirtualAudioOutput(devices[index])', outputs)
        apply = source.split('AudioDeviceService::applyOutputSetup', 1)[1]
        self.assertIn('isVirtualAudioOutput(outputName)', apply)

    def test_probe_buffers_are_checked_before_clear(self):
        source = (ROOT / 'scripts/patch-juce-wasapi-raw.cmake').read_text(encoding='utf-8')
        for index in (0, 1):
            self.assertIn(f'if (auto* buffer = bufferInfos[outputBufferIndex + i].buffers[{index}])', source)

    def test_recovery_message_requires_verified_restore(self):
        source = (ROOT / 'native/Source/MainComponent.cpp').read_text(encoding='utf-8')
        apply = source.split('void MainComponent::applyAudioSettingsFromWeb', 1)[1].split(
            'void MainComponent::showDeviceSettings', 1)[0]
        self.assertIn('bool restoredPrevious = false', apply)
        self.assertIn('restoredPrevious ?', apply)
        self.assertNotIn('(void) audio.applyOutputSetup', apply)


if __name__ == '__main__':
    unittest.main()
