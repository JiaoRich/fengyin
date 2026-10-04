from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[1]


class AsioBridgeModeTests(unittest.TestCase):
    def test_asio_is_compiled_but_only_supported_bridge_is_exposed(self):
        cmake = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
        audio = (ROOT / "native" / "Source" / "AudioDeviceService.cpp").read_text(encoding="utf-8")
        self.assertIn("JUCE_ASIO=1", cmake)
        self.assertIn('containsIgnoreCase("Synchronous Audio Router")', audio)
        self.assertIn("isSupportedBridgeDevice(devices[index])", audio)
        self.assertIn("桥接测试模式只能选择 Synchronous Audio Router", audio)

    def test_automatic_mode_still_only_prefers_windows_shared_audio(self):
        audio = (ROOT / "native" / "Source" / "AudioDeviceService.cpp").read_text(encoding="utf-8")
        preferred = audio[
            audio.index("AudioDeviceService::preferredLiveDeviceType"):
            audio.index("AudioDeviceService::configureAutomaticType")
        ]
        self.assertIn("Low Latency Mode", preferred)
        self.assertNotIn("isAsioType", preferred)

    def test_ui_marks_bridge_as_manual_test_mode(self):
        html = (ROOT / "prototype" / "index.html").read_text(encoding="utf-8")
        js = (ROOT / "prototype" / "app.js").read_text(encoding="utf-8")
        self.assertIn('id="bridge-test-status"', html)
        self.assertIn("桥接低延迟测试（ASIO）", js)
        self.assertIn("state?.bridgeAvailable", js)

    def test_bridge_configuration_and_input_mix_are_implemented(self):
        audio = (ROOT / "native" / "Source" / "AudioDeviceService.cpp").read_text(encoding="utf-8")
        main = (ROOT / "native" / "Source" / "MainComponent.cpp").read_text(encoding="utf-8")
        html = (ROOT / "prototype" / "index.html").read_text(encoding="utf-8")
        self.assertIn("showControlPanel()", audio)
        self.assertIn("createDevice(sarName, sarName)", audio)
        self.assertIn("bridgePlaybackInputEnabled", audio)
        self.assertIn("getActiveInputChannels", audio)
        self.assertIn("FloatVectorOperations::copy", audio)
        self.assertIn('configureAudioBridge', main)
        self.assertIn('configure-audio-bridge', html)

    def test_latency_display_does_not_double_count_same_output_period(self):
        audio = (ROOT / "native" / "Source" / "AudioDeviceService.cpp").read_text(encoding="utf-8")
        self.assertIn("juce::jmax(status.bufferSize, reported)", audio)
        self.assertNotIn("status.bufferSize + device->getOutputLatencyInSamples()", audio)


if __name__ == "__main__":
    unittest.main()
