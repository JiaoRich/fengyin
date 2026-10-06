import pathlib
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]

class VBCableTrialTests(unittest.TestCase):
    def read(self, name):
        return (ROOT / name).read_text(encoding="utf-8-sig")

    def test_capture_is_not_render_loopback_in_cable_mode(self):
        source = self.read("audio-engine/windows/WasapiLoopbackInput.cpp")
        self.assertIn("vbCableTrial() ? eCapture : eRender", source)
        self.assertIn("AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM", source)
        self.assertIn("streamFormat", source)
        self.assertIn("cableFormat.nSamplesPerSec = engineSampleRate", source)

    def test_launch_is_explicit_and_process_local(self):
        source = self.read("scripts/vbcable-trial/Start-VBCable-Test.cmd")
        self.assertTrue(source.isascii())
        self.assertIn("setlocal", source)
        self.assertIn('set "FENGYIN_VBCABLE_TEST=1"', source)
        self.assertNotIn("setx", source.lower())
        self.assertIn('IMAGENAME eq FengYin.exe', source)

    def test_asio_stays_physical_and_failure_is_logged(self):
        source = self.read("audio-engine/windows/Asio4AllOutput.cpp")
        self.assertIn('containsIgnoreCase("cable")', source)
        source = self.read("audio-engine/windows/AudioEngineMain.cpp")
        self.assertIn("Asio4AllOutput output", source)
        self.assertIn("System capture failed:", source)

    def test_route_choice_shared_with_availability(self):
        for name in ["audio-engine/windows/DefaultEndpointRouter.cpp",
                     "audio-engine/client/AudioEngineProcessController.cpp"]:
            self.assertIn("matchesVirtualEndpoint", self.read(name))

if __name__ == "__main__":
    unittest.main()
