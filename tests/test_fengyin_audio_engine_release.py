import pathlib
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[1]


class FengYinAudioEngineReleaseTests(unittest.TestCase):
    def read(self, relative):
        return (ROOT / relative).read_text(encoding="utf-8-sig")

    def test_public_activation_requires_installed_virtual_endpoint(self):
        service = self.read("native/Source/AudioDeviceService.cpp")
        self.assertIn("AudioEngineProcessController::isAvailable()", service)
        self.assertIn('getBoolValue("audioEngineEnabled", true)', service)
        self.assertIn("startIsolatedAudioEngine(requestedFrames)", service)

    def test_engine_tries_safe_periods_and_supports_oem_alignment(self):
        engine = self.read("audio-engine/windows/AudioEngineMain.cpp")
        device = self.read("audio-engine/client/FengYinEngineAudioIODevice.cpp")
        self.assertIn("{ 128u, 256u, 512u }", engine)
        self.assertIn("bufferSizeSamples > maximumFramesPerBlock", device)

    def test_driver_is_render_only_and_has_stable_hardware_id(self):
        inf = self.read("audio-driver/sysvad-patch/FengYinAudio.inx")
        self.assertIn(r"Root\FengYinAudioEngine", inf)
        self.assertIn('FengYin.SpeakerName = "风吟共享扬声器"', inf)
        self.assertNotIn("KSCATEGORY_CAPTURE", inf)
        self.assertNotIn("WaveMic", inf)

    def test_ci_keeps_driver_validation_enabled(self):
        workflow = self.read(".github/workflows/audio-driver-build.yml")
        self.assertIn("ApiValidator.exe", workflow)
        self.assertIn("infverif.exe", workflow)
        self.assertNotIn("RunApiValidator=false", workflow)

    def test_public_package_rejects_test_signed_driver(self):
        release = self.read("scripts/check-release.ps1")
        installer = self.read("installer/FengYin.iss")
        self.assertIn("WDKTestCert", release)
        self.assertIn("Get-AuthenticodeSignature", release)
        self.assertIn("FengYinDriverSetup.exe", installer)
        self.assertIn("--uninstall", installer)

    def test_endpoint_route_has_crash_and_hotplug_recovery(self):
        router = self.read("audio-engine/windows/DefaultEndpointRouter.cpp")
        engine = self.read("audio-engine/windows/AudioEngineMain.cpp")
        self.assertIn("writeJournal(previousEndpointIds)", router)
        self.assertIn("restorePendingRoute", router)
        self.assertIn("pollPhysicalDefaultChange", engine)


if __name__ == "__main__":
    unittest.main()
