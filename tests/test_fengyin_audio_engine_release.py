import pathlib
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[1]


class FengYinAudioEngineReleaseTests(unittest.TestCase):
    def test_asio_engine_does_not_probe_microphone_during_initialisation(self):
        patch = self.read("scripts/patch-juce-wasapi-raw.cmake")
        cmake = self.read("CMakeLists.txt")
        self.assertIn("FENGYIN_ASIO_RENDER_ONLY", patch)
        self.assertIn("FENGYIN_ASIO_RENDER_ONLY=1 JUCE_ASIO_DEBUGGING=1", cmake)

    def test_engine_rejects_disconnected_outputs_and_reattaches_client(self):
        backend = self.read("audio-engine/windows/Asio4AllOutput.cpp")
        self.assertIn('containsIgnoreCase("Not Connected")', backend)
        self.assertLess(backend.index('containsIgnoreCase("Not Connected")'),
                        backend.index("device->start(this)"))
        service = self.read("native/Source/AudioDeviceService.cpp")
        start = service.split("juce::String AudioDeviceService::startIsolatedAudioEngine", 1)[1]
        start = start.split("void AudioDeviceService::timerCallback", 1)[0]
        self.assertNotIn("if (engineProcess.isRunning()) return {};", start)
        self.assertIn("manager.initialise(0, 2, &engineState, false)", start)

    def read(self, relative):
        return (ROOT / relative).read_text(encoding="utf-8-sig")

    def test_public_activation_requires_installed_virtual_endpoint(self):
        service = self.read("native/Source/AudioDeviceService.cpp")
        self.assertIn("AudioEngineProcessController::isAvailable()", service)
        self.assertIn('getBoolValue("audioEngineEnabled", true)', service)
        self.assertIn("startIsolatedAudioEngine(requestedFrames)", service)

    def test_engine_uses_asio4all_without_silent_backend_or_period_fallback(self):
        engine = self.read("audio-engine/windows/AudioEngineMain.cpp")
        device = self.read("audio-engine/client/FengYinEngineAudioIODevice.cpp")
        self.assertIn("Asio4AllOutput output", engine)
        self.assertNotIn("WasapiExclusiveOutput", engine)
        self.assertNotIn("periodCandidates", engine)
        backend = self.read("audio-engine/windows/Asio4AllOutput.cpp")
        self.assertIn("createAudioIODeviceType_ASIO", backend)
        self.assertIn('containsIgnoreCase("ASIO4ALL")', backend)
        self.assertIn("lastCallback.load() == 0", backend)
        self.assertIn("runDispatchLoopUntil", engine)
        self.assertIn("WaitForSingleObject(requestSemaphore, 0)", device)
        self.assertIn("physicalOutputLatencyFrames", device)
        self.assertIn("bufferSizeSamples > maximumFramesPerBlock", device)

    def test_driver_is_render_only_and_has_stable_hardware_id(self):
        inf = self.read("audio-driver/sysvad-patch/FengYinAudio.inx")
        self.assertIn(r"Root\FengYinAudioEngine", inf)
        self.assertIn("NTamd64.10.0...16299", inf)
        self.assertIn('FengYin.SpeakerName = "风吟共享扬声器"', inf)
        self.assertNotIn("KSCATEGORY_CAPTURE", inf)
        self.assertNotIn("WaveMic", inf)
        self.assertIn("D:P(A;;GA;;;SY)", inf)

    def test_driver_targets_downlevel_windows_and_verifies_real_endpoint(self):
        prepare = self.read("scripts/prepare-fengyin-audio-driver.ps1")
        setup = self.read("audio-engine/windows/DriverSetupMain.cpp")
        install = self.read("scripts/audio-test-kit/Install-TestAudioEngine.ps1")
        uninstall = self.read("scripts/audio-test-kit/Uninstall-TestAudioEngine.ps1")
        self.assertIn("10.0.26100.1", prepare)
        self.assertIn("Directory.Build.props", prepare)
        self.assertIn("<KMDF_VERSION_MINOR>15</KMDF_VERSION_MINOR>", prepare)
        self.assertIn("<_NT_TARGET_VERSION>0xA00000B</_NT_TARGET_VERSION>", prepare)
        self.assertNotIn("NTDDI_VERSION=NTDDI_WIN10_VB", prepare)
        workflow = self.read(".github/workflows/audio-driver-build.yml")
        self.assertGreaterEqual(workflow.count("runs-on: windows-2022"), 2)
        self.assertIn("ExAllocateFromNPagedLookasideList", workflow)
        self.assertIn("ExFreeToNPagedLookasideList", workflow)
        self.assertIn("CM_Get_DevNode_Status", setup)
        self.assertIn("EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE", setup)
        self.assertIn("waitForOperationalInstallation", setup)
        self.assertIn("$InstallResult -eq 10", install)
        self.assertIn("--check-device", uninstall)

    def test_ci_keeps_driver_validation_enabled(self):
        workflow = self.read(".github/workflows/audio-driver-build.yml")
        self.assertIn("apivalidator.exe", workflow.lower())
        self.assertIn("infverif.exe", workflow.lower())
        self.assertIn("inf2cat.exe", workflow.lower())
        self.assertNotIn("RunApiValidator=false", workflow)

    def test_free_engine_trial_is_isolated_from_public_release(self):
        workflow = self.read(".github/workflows/audio-driver-build.yml")
        test_validator = self.read("scripts/test-fengyin-test-driver.ps1")
        production_validator = self.read("scripts/test-fengyin-production-driver.ps1")
        packager = self.read("scripts/package-audio-engine-test-kit.ps1")
        self.assertIn("signtool", workflow.lower())
        self.assertIn("FengYinAudio.cat", workflow)
        self.assertIn("WDKTestCert", test_validator)
        self.assertIn("WDKTestCert", production_validator)
        self.assertIn("test-driver", packager)
        self.assertIn("UTF8Encoding]::new($true)", packager)
        self.assertIn("powershell.exe -NoProfile -NonInteractive", packager)
        self.assertNotIn("package-audio-engine-test-kit.ps1", self.read("scripts/build-windows.ps1"))

    def test_free_engine_trial_has_complete_rollback(self):
        install = self.read("scripts/audio-test-kit/Install-TestAudioEngine.ps1")
        uninstall = self.read("scripts/audio-test-kit/Uninstall-TestAudioEngine.ps1")
        disable = self.read("scripts/audio-test-kit/Disable-TestMode.ps1")
        self.assertIn("TrustedPublisher", install)
        self.assertIn("--recover-only", uninstall)
        self.assertIn("--uninstall", uninstall)
        self.assertIn("testsigning off", disable)

    def test_trial_launchers_use_ascii_script_paths_and_keep_errors_visible(self):
        expected = {
            "scripts/audio-test-kit/1-开启测试模式.cmd": "Enable-TestMode.ps1",
            "scripts/audio-test-kit/2-安装测试音频引擎.cmd": "Install-TestAudioEngine.ps1",
            "scripts/audio-test-kit/3-卸载并恢复系统声音.cmd": "Uninstall-TestAudioEngine.ps1",
            "scripts/audio-test-kit/4-关闭测试模式.cmd": "Disable-TestMode.ps1",
        }
        for launcher, script_name in expected.items():
            contents = self.read(launcher)
            self.assertIn(script_name, contents)
            self.assertIn("pause", contents.lower())

    def test_public_package_rejects_test_signed_driver(self):
        release = self.read("scripts/check-release.ps1")
        validator = self.read("scripts/test-fengyin-production-driver.ps1")
        installer = self.read("installer/FengYin.iss")
        self.assertIn("test-fengyin-production-driver.ps1", release)
        self.assertIn("Get-AuthenticodeSignature", validator)
        self.assertNotIn("FengYinDriverSetup.exe", installer)
        self.assertNotIn("HasAudioDriver", installer)
        self.assertNotIn("CurUninstallStepChanged", installer)
        self.assertIn("HKLM64", installer)
        self.assertIn("InprocServer32", installer)
        self.assertIn("FileExists(RemoveQuotes(Server))", installer)
        self.assertIn("Tasks: asio4all; Check: NeedsASIO4ALL", installer)
        self.assertNotIn("FengYinDriverSetup.exe", self.read("scripts/build-windows.ps1"))

    def test_public_package_bundles_and_installs_fresh_air(self):
        build = self.read("scripts/build-windows.ps1")
        prepare = self.read("scripts/prepare-fresh-air.ps1")
        release = self.read("scripts/check-release.ps1")
        installer = self.read("installer/FengYin.iss")
        expected_hash = "C50D3CE92ACB7524B1A4C962F9ADFCCE100FBEB957715B8788051D73F0D02A73"
        self.assertIn("prepare-fresh-air.ps1", build)
        self.assertIn(expected_hash, prepare)
        self.assertIn(expected_hash, release)
        self.assertIn('Name: "freshair"', installer)
        self.assertIn("NeedsFreshAir", installer)
        self.assertIn(r"VST3\Fresh Air.vst3", installer)
        self.assertIn(r"VST3\Slate Digital\Fresh Air.vst3", installer)
        self.assertIn("/VERYSILENT /SUPPRESSMSGBOXES /NORESTART /SP-", installer)
        self.assertIn("AfterInstall: VerifyFreshAirInstalled", installer)
        self.assertIn("RaiseException", installer)

    def test_public_package_uses_verified_offline_webview_runtime(self):
        build = self.read("scripts/build-windows.ps1")
        release = self.read("scripts/check-release.ps1")
        installer = self.read("installer/FengYin.iss")
        expected_name = "MicrosoftEdgeWebView2RuntimeInstallerX64.exe"
        self.assertIn("linkid=2124701", build)
        self.assertIn(expected_name, build)
        self.assertIn(expected_name, release)
        self.assertIn(expected_name, installer)
        self.assertIn("Get-AuthenticodeSignature", build)
        self.assertIn("Get-AuthenticodeSignature", release)
        self.assertIn("Microsoft Corporation", build)
        self.assertIn("Microsoft Corporation", release)
        self.assertNotIn("MicrosoftEdgeWebview2Setup.exe", installer)

    def test_release_tone_catalog_is_explicit_and_all_embedded_assets_exist(self):
        catalog = self.read("native/Source/PublishedToneCatalog.h")
        expected_names = [
            "高萨-肯萨", "高萨-流行", "中萨-爵士", "中萨-深情", "中萨-流行",
            "次中萨-醇厚", "次中萨-温暖", "次中萨-气包音", "上低萨-爵士", "上低萨-流行",
            "小号", "高音小号", "中音长号", "低音长号", "长笛", "短笛", "单簧管", "双簧管", "巴松管",
            "小提琴独奏", "中提琴独奏", "大提琴独奏", "小提琴重奏", "中提琴重奏", "大提琴重奏",
            "二胡", "古筝", "葫芦丝", "柳琴", "马头琴", "曲笛", "埙", "唢呐", "三弦", "琵琶", "笙", "南箫",
        ]
        self.assertIn("std::array<PublishedToneDefinition, 37>", catalog)
        for name in expected_names:
            self.assertEqual(catalog.count(f'"{name}"'), 1, name)
        self.assertNotIn("扫描本机", catalog)
        assets = [ROOT / "assets/tone-packages/tenor-air-pocket.fytonepack"]
        assets.extend((ROOT / "assets/tone-packages/kong").glob("*.kam"))
        self.assertEqual(len(assets), 13)
        for asset in assets:
            self.assertTrue(asset.is_file() and asset.stat().st_size > 300, asset)

    def test_kong_release_routes_multichannel_techniques(self):
        host = self.read("native/Source/PluginHostEngine.cpp")
        midi = self.read("native/Source/MidiInputService.cpp")
        self.assertIn('key == "kong-suona"', host)
        self.assertIn('key == "kong-dizi"', host)
        self.assertIn("switchPerformanceChannel", host)
        self.assertIn("route(PerformanceTechnique::flutter, 2, -1)", host)
        self.assertIn("route(PerformanceTechnique::tremolo, 3, -1)", host)
        self.assertIn('techniqueContext == "kong-suona"', midi)
        self.assertIn('techniqueContext == "kong-pipa"', midi)

    def test_production_driver_handoff_is_strict_and_repeatable(self):
        prepare = self.read("scripts/prepare-driver-submission.ps1")
        importer = self.read("scripts/import-signed-driver.ps1")
        validator = self.read("scripts/test-fengyin-production-driver.ps1")
        workflow = self.read(".github/workflows/audio-driver-build.yml")
        self.assertIn("TabletAudioSample.pdb", prepare)
        self.assertIn("DestinationDir=$PackageName", prepare)
        self.assertIn("FengYinAudio-attestation.cab", workflow)
        self.assertIn("Root\\FengYinAudioEngine", validator)
        self.assertIn("Microsoft Windows Hardware Compatibility", validator)
        self.assertIn("WDKTestCert", validator)
        self.assertIn("test-fengyin-production-driver.ps1", importer)

    def test_windows_install_acceptance_covers_crash_restore_and_cleanup(self):
        acceptance = self.read("scripts/test-audio-engine-install.ps1")
        self.assertIn("--route-system-audio", acceptance)
        self.assertIn("--recover-only", acceptance)
        self.assertIn("--uninstall", acceptance)

    def test_driver_change_stops_engine_and_restores_windows_route_first(self):
        setup = self.read("audio-engine/windows/DriverSetupMain.cpp")
        cmake = self.read("CMakeLists.txt")
        self.assertIn("stopAudioEngine()", setup)
        self.assertIn("DefaultEndpointRouter::restorePendingRoute", setup)
        self.assertIn("if (! prepareForDriverChange()) return 9", setup)
        self.assertIn("audio-engine/windows/DefaultEndpointRouter.cpp", cmake)

    def test_endpoint_route_has_crash_and_hotplug_recovery(self):
        router = self.read("audio-engine/windows/DefaultEndpointRouter.cpp")
        engine = self.read("audio-engine/windows/AudioEngineMain.cpp")
        self.assertIn("writeJournal(previousEndpointIds)", router)
        self.assertIn("restorePendingRoute", router)
        self.assertIn("pollPhysicalDefaultChange", engine)


if __name__ == "__main__":
    unittest.main()
