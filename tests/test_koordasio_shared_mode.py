from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[1]


class KoordAsioSharedModeTests(unittest.TestCase):
    def test_asio_is_compiled_but_only_koordasio_is_exposed(self):
        cmake = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
        audio = (ROOT / "native" / "Source" / "AudioDeviceService.cpp").read_text(encoding="utf-8")
        self.assertIn("JUCE_ASIO=1", cmake)
        self.assertIn('containsIgnoreCase("KoordASIO")', audio)
        self.assertIn("isSupportedSharedAsioDevice(devices[index])", audio)
        self.assertIn("共享低延迟模式只能选择 KoordASIO", audio)
        self.assertNotIn("Synchronous Audio Router", audio)

    def test_automatic_mode_still_only_prefers_windows_shared_audio(self):
        audio = (ROOT / "native" / "Source" / "AudioDeviceService.cpp").read_text(encoding="utf-8")
        preferred = audio[
            audio.index("AudioDeviceService::preferredLiveDeviceType"):
            audio.index("AudioDeviceService::configureAutomaticType")
        ]
        self.assertIn("Low Latency Mode", preferred)
        self.assertNotIn("isAsioType", preferred)

    def test_koordasio_is_output_only_and_needs_no_virtual_endpoint(self):
        header = (ROOT / "native" / "Source" / "AudioDeviceService.h").read_text(encoding="utf-8")
        audio = (ROOT / "native" / "Source" / "AudioDeviceService.cpp").read_text(encoding="utf-8")
        main = (ROOT / "native" / "Source" / "MainComponent.cpp").read_text(encoding="utf-8")
        html = (ROOT / "prototype" / "index.html").read_text(encoding="utf-8")
        self.assertNotIn("AudioIODeviceCallback", header)
        self.assertNotIn("configureBridgePlaybackEndpoint", audio)
        self.assertNotIn("bridgePlaybackInputEnabled", audio)
        self.assertNotIn("configureAudioBridge", main)
        self.assertNotIn("configure-audio-bridge", html)
        self.assertIn("setup.inputDeviceName.clear()", audio)
        self.assertIn("setup.inputChannels.clear()", audio)

    def test_selecting_asio_opens_koord_directly_not_default_asio_driver(self):
        audio = (ROOT / "native" / "Source" / "AudioDeviceService.cpp").read_text(encoding="utf-8")
        select = audio[audio.index("AudioDeviceService::selectDeviceType"):
                       audio.index("AudioDeviceService::applyOutputSetup")]
        self.assertIn('koordState.setAttribute("deviceType", "ASIO")', select)
        self.assertIn('koordState.setAttribute("audioOutputDeviceName", devices[0])', select)
        self.assertIn("manager.initialise(0, 2, &koordState, false)", select)
        asio_branch = select[select.index("if (isAsioType(typeName))"):select.index("manager.setCurrentAudioDeviceType")]
        self.assertNotIn("manager.setCurrentAudioDeviceType(", asio_branch)

    def test_production_ui_replaces_koordasio_trial_with_vbcable(self):
        html = (ROOT / "prototype" / "index.html").read_text(encoding="utf-8")
        js = (ROOT / "prototype" / "app.js").read_text(encoding="utf-8")
        self.assertNotIn('id="shared-asio-status"', html)
        self.assertIn("VB-CABLE 接收网页声音", js)
        self.assertNotIn('id="download-vbcable"', html)
        self.assertNotIn("state?.sharedAsioAvailable", js)

    def test_failed_buffer_change_restores_exact_koordasio_setup(self):
        audio = (ROOT / "native" / "Source" / "AudioDeviceService.cpp").read_text(encoding="utf-8")
        main = (ROOT / "native" / "Source" / "MainComponent.cpp").read_text(encoding="utf-8")
        self.assertIn("const auto previousSetup = manager.getAudioDeviceSetup()", audio)
        self.assertIn("manager.setAudioDeviceSetup(previousSetup, true)", audio)
        self.assertIn("已保留 KoordASIO 并恢复原缓冲区", audio)
        self.assertIn("const auto previousWasSharedAsio", main)
        self.assertIn("error.isNotEmpty() && ! previousWasSharedAsio", main)

    def test_latency_display_does_not_double_count_same_output_period(self):
        audio = (ROOT / "native" / "Source" / "AudioDeviceService.cpp").read_text(encoding="utf-8")
        self.assertIn("juce::jmax(status.bufferSize, reported)", audio)
        self.assertNotIn("status.bufferSize + device->getOutputLatencyInSamples()", audio)


if __name__ == "__main__":
    unittest.main()
