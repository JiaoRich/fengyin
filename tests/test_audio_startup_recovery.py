from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]


class AudioStartupRecovery(unittest.TestCase):
    def test_constructor_reset_survives_timer_cancellation(self):
        source = (ROOT / 'scripts/patch-juce-wasapi-raw.cmake').read_text(encoding='utf-8')
        self.assertIn('std::atomic<bool> fengyinResetPending', source)
        self.assertIn('fengyinResetPending.store (true)', source)
        self.assertIn('fengyinResetPending.exchange (false)', source)
        handling = source.split('if (fengyinResetPending.exchange (false))', 1)[1]
        self.assertLess(handling.index('initDriver()'), handling.index('asioObject->getChannels'))
        self.assertLess(handling.index('reloadChannelNames()'), handling.index('asioObject->getChannels'))

    def test_blocked_hotplug_recovery_has_parent_deadline(self):
        source = (ROOT / 'native/Source/AudioDeviceService.cpp').read_text(encoding='utf-8')
        timer = source.split('void AudioDeviceService::timerCallback()', 1)[1].split('AudioDeviceStatus AudioDeviceService::getStatus()', 1)[0]
        self.assertIn('engineProcess.isRecovering()', timer)
        self.assertIn('now - engineRecoveryStartedAt < 15000.0', timer)
        self.assertLess(timer.index('engineProcess.stop()'), timer.index('restorePhysicalSharedOutput()'))

    def test_hotplug_recovery_publishes_failure_instead_of_stale_running(self):
        source = (ROOT / 'audio-engine/windows/AudioEngineMain.cpp').read_text(encoding='utf-8')
        hotplug = source.split('Physical output topology changed; reopening ASIO4ALL', 1)[1].split('if (output.isRunning())', 1)[0]
        self.assertIn('StreamState::recovering, 0', hotplug)
        self.assertIn('outputRecovered = output.start(core,requestedFrames,previous,routeError)', hotplug)
        self.assertIn('!outputRecovered || output.actualBufferFrames() != actualFrames', hotplug)
        self.assertIn('StreamState::fallback, 0', hotplug)
        self.assertIn('discardQueuedAudio(*instrumentMapping.get())', hotplug)

    def test_ordinary_startup_preserves_driver_configuration(self):
        source = (ROOT / 'audio-engine/windows/Asio4AllOutput.cpp').read_text(encoding='utf-8')
        self.assertIn('selectedEndpoint = requireMatch && !preferredEndpoint.empty()', source)
        self.assertNotIn('refreshAsioEndpoints(', source)
        self.assertIn('ASIO endpoint matching skipped for ordinary startup', source)
        self.assertIn('channelNames[index].containsIgnoreCase("cable")', source)

    def test_driver_start_and_callback_wait_are_distinguishable(self):
        source = (ROOT / 'scripts/patch-juce-wasapi-raw.cmake').read_text(encoding='utf-8')
        self.assertIn('driver start returned: code=', source)
        self.assertIn('first callback wait finished: callbackSeen=', source)

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
