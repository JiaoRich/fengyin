from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]


class WasapiRawPatchTests(unittest.TestCase):
    def test_patch_is_scoped_and_explicit(self):
        patch = (ROOT / "scripts" / "patch-juce-wasapi-raw.cmake").read_text(encoding="utf-8")
        self.assertIn("sharedLowLatencyRaw", patch)
        self.assertIn("AUDCLNT_STREAMOPTIONS_RAW", patch)
        self.assertIn("SetClientProperties", patch)
        self.assertIn("AudioCategory_Media", patch)
        self.assertNotIn("PKEY_AudioEndpoint_Disable_SysFx", patch)

    def test_raw_is_not_the_automatic_default(self):
        audio = (ROOT / "native" / "Source" / "AudioDeviceService.cpp").read_text(encoding="utf-8")
        preferred = audio[audio.index("AudioDeviceService::preferredLiveDeviceType"):audio.index("AudioDeviceService::configureAutomaticType")]
        self.assertNotIn("RAW Test Mode", preferred)
        ui = (ROOT / "prototype" / "app.js").read_text(encoding="utf-8")
        self.assertNotIn("RAW测试模式（共享）", ui)
        self.assertIn("不修改驱动、不独占设备", ui)
