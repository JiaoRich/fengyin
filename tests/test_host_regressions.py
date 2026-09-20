import pathlib
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[1]
HOST = (ROOT / "native" / "Source" / "PluginHostEngine.cpp").read_text(encoding="utf-8")
MIDI = (ROOT / "native" / "Source" / "MidiInputService.cpp").read_text(encoding="utf-8")
MAIN = (ROOT / "native" / "Source" / "MainComponent.cpp").read_text(encoding="utf-8")
JS = (ROOT / "prototype" / "app.js").read_text(encoding="utf-8")
MASTER = (ROOT / "native" / "Source" / "MasterOutputService.cpp").read_text(encoding="utf-8")
PRESET = (ROOT / "native" / "Source" / "SoundPresetStore.cpp").read_text(encoding="utf-8")
PITCH_KEY = (ROOT / "native" / "Source" / "PitchKey.h").read_text(encoding="utf-8")


class HostRegressionTests(unittest.TestCase):
    def test_graph_has_stereo_output_before_io_nodes_are_connected(self):
        configure = HOST.index("graph->setPlayConfigDetails(0, 2, sampleRate, bufferSize)")
        output_node = HOST.index("audioOutputNode = graph->addNode", configure)
        rebuild = HOST.index("if (! rebuildConnections())", output_node)
        self.assertLess(configure, output_node)
        self.assertLess(output_node, rebuild)
        self.assertIn("return midiConnected && audioConnections > 0", HOST)

    def test_midi_hotplug_status_is_polled_and_cleared(self):
        self.assertIn("void MidiInputService::pollConnection()", MIDI)
        self.assertIn("midi.pollConnection();", MAIN)
        self.assertIn("lastNote.store(-1", MIDI)
        self.assertIn("'已收到气息，尚未收到音符'", JS)
        preferred_block = MIDI.split("if (preferred.isNotEmpty())", 1)[1].split("if (! devices.isEmpty())", 1)[0]
        self.assertNotIn("        return;\n    }", preferred_block)

    def test_safe_onset_resets_expression_and_caps_velocity(self):
        self.assertIn("sink->resetPerformance()", MIDI)
        self.assertIn("sink->breathChanged(0.035f)", MIDI)
        self.assertIn("0.28f + currentBreath * 0.52f", MIDI)
        self.assertIn('activeProfile.id == "yamaha-yds"', MIDI)
        self.assertIn("savedController = 11", MIDI)
        self.assertIn("juce::MidiMessage::allNotesOff", HOST)
        self.assertIn("controllerEvent(1, 11, 0)", HOST)

    def test_transpose_releases_active_notes_and_uses_note_map(self):
        self.assertIn("void MidiInputService::setTransposeSemitones", MIDI)
        self.assertIn("transposeFromTo", PITCH_KEY)
        self.assertIn("PitchKey::transposeFromTo", MIDI)
        self.assertIn("keyCalibrationPending.exchange", MIDI)
        self.assertIn("keyCalibrated.store(true", MIDI)
        self.assertIn("keyCalibrated.load", MIDI)
        self.assertIn("transposeSemitones.store(0", MIDI)
        connect_block = MIDI.split("bool MidiInputService::connect", 1)[1].split("void MidiInputService::disconnect", 1)[0]
        self.assertIn("keyCalibrationPending.store(false", connect_block)
        self.assertNotIn("keyCalibrationPending.store(true", connect_block)
        self.assertNotIn('setValue(sourceKeySettingKey()', MIDI)
        self.assertIn("activeOutputNotes", MIDI)
        self.assertIn("sourceNote + transposeSemitones.load", MIDI)
        self.assertIn("sink->noteOff(outputNote)", MIDI)

    def test_machine_code_copy_and_calibration_gate_exist(self):
        self.assertIn('id="copy-machine-code"', (ROOT / "prototype" / "index.html").read_text(encoding="utf-8"))
        self.assertIn("nativeEvent('copyMachineCode')", JS)
        self.assertIn('id="key-calibration-dialog"', (ROOT / "prototype" / "index.html").read_text(encoding="utf-8"))
        self.assertIn("if (!latestBackendState.keyCalibrated)", JS)
        self.assertIn("请按平时演奏 C（Do）的指法，吹一个音", JS)
        self.assertIn("cancelKeyCalibration", MAIN)
        self.assertIn("copyMachineCode", MAIN)

    def test_video_playback_prioritises_realtime_audio(self):
        accompaniment = (ROOT / "native" / "Source" / "AccompanimentAudioService.cpp").read_text(encoding="utf-8")
        video = (ROOT / "native" / "Source" / "VideoPlayerPanel.cpp").read_text(encoding="utf-8")
        audio_device = (ROOT / "native" / "Source" / "AudioDeviceService.cpp").read_text(encoding="utf-8")
        self.assertIn("accompanimentReadAheadSamples = 262144", accompaniment)
        self.assertIn("sustainedDriftChecks >= 5", video)
        self.assertIn("videoPlaybackActive ? 6 : 3", MAIN)
        self.assertIn("setVideoPlaybackState", JS)
        self.assertIn("followSystemDefaultOutput", audio_device)
        self.assertIn("getDefaultDeviceIndex(false)", audio_device)
        self.assertIn("current->getTypeName().containsIgnoreCase(\"ASIO\")", audio_device)
        self.assertIn("audio.followSystemDefaultOutput()", MAIN)
        refresh = MAIN.index("videoPlaybackActive ? 6 : 3")
        self.assertGreater(MAIN.index("masterOutput.getSpectrum", refresh), refresh)

    def test_initial_audio_setup_prefers_low_latency_without_overriding_manual_choice(self):
        audio_device = (ROOT / "native" / "Source" / "AudioDeviceService.cpp").read_text(encoding="utf-8")
        self.assertIn("applyBestInitialSetup", audio_device)
        self.assertIn('getValue("audioSetupMode") == "manual"', audio_device)
        self.assertIn('containsIgnoreCase("Low Latency Mode")', audio_device)
        self.assertIn('getDefaultDeviceIndex(false)', audio_device)
        self.assertIn('automaticAudioDevice', audio_device)
        self.assertIn("setup.sampleRate = 48000.0", audio_device)
        self.assertIn("setup.bufferSize = 128", audio_device)
        self.assertIn("setup.bufferSize = 256", audio_device)
        self.assertIn("audio.applyBestInitialSetup();", MAIN)
        self.assertIn('withEventListener("applyAudioSettings"', MAIN)
        self.assertIn('withEventListener("requestAudioSettings"', MAIN)
        self.assertIn('emitEventIfBrowserIsVisible("audioSettingsState"', MAIN)

    def test_audio_setup_uses_real_driver_capabilities_and_runtime_stability(self):
        audio_device = (ROOT / "native" / "Source" / "AudioDeviceService.cpp").read_text(encoding="utf-8")
        self.assertIn("recommendedInitialBuffer", audio_device)
        self.assertIn('containsIgnoreCase("ASIO4ALL")', audio_device)
        self.assertIn("getAvailableBufferSizes()", audio_device)
        self.assertIn("manager.getCpuUsage() >= 0.82", audio_device)
        self.assertIn("audio.stabiliseAfterXRuns()", MAIN)

    def test_first_run_audio_tuning_is_automatic_but_manual_controls_remain(self):
        audio_header = (ROOT / "native" / "Source" / "AudioDeviceService.h").read_text(encoding="utf-8")
        audio_device = (ROOT / "native" / "Source" / "AudioDeviceService.cpp").read_text(encoding="utf-8")
        self.assertIn("beginAutomaticLatencyTuning", audio_header)
        self.assertIn("pollAutomaticLatencyTuning", audio_header)
        self.assertIn("latencyCandidateTestMs", audio_device)
        self.assertIn('containsIgnoreCase("Exclusive")', audio_device)
        self.assertIn("addedXRuns == 0", audio_device)
        self.assertIn("tuningCandidateMaximumCpu", audio_device)
        self.assertIn('automaticLatencyTunedDevice', audio_device)
        self.assertIn("audio.beginAutomaticLatencyTuning();", MAIN)
        self.assertIn('withEventListener("applyAudioSettings"', MAIN)

    def test_three_day_trial_unlocks_features_and_expires_safely(self):
        license_header = (ROOT / "native" / "Source" / "LicenseService.h").read_text(encoding="utf-8")
        license_source = (ROOT / "native" / "Source" / "LicenseService.cpp").read_text(encoding="utf-8")
        self.assertIn("trialActive", license_header)
        self.assertIn("trialExpired", license_header)
        self.assertIn("startTrial", license_source)
        self.assertIn("3 * 24 * 60 * 60", license_source)
        self.assertIn("clockRolledBack", license_source)
        self.assertIn('withEventListener("startTrial"', MAIN)
        self.assertIn("currentLicenseStatus.canUseFeatures()", MAIN)
        self.assertIn("midi.setPerformanceSink(nullptr)", MAIN)

    def test_connection_runs_automatic_breath_adaptation(self):
        self.assertIn("automaticBreathDetectionActive = true", MAIN)
        self.assertIn("midi.beginBreathDetection()", MAIN)
        self.assertIn("midi.finishBreathDetection()", MAIN)
        self.assertIn("automaticBreathDetection", MAIN)

    def test_technique_learning_and_swam_parameter_control_are_connected(self):
        self.assertIn("beginTechniqueLearn", MIDI)
        self.assertIn("handleTechniqueMessage", MIDI)
        self.assertIn("resolveTechniqueParameters", HOST)
        self.assertIn('name.contains("growl")', HOST)
        self.assertIn('name.contains("flutter")', HOST)
        self.assertIn("setValueNotifyingHost", HOST)
        self.assertIn("setTechniqueContext", MIDI)
        self.assertIn("techniqueSettingKey", MIDI)
        self.assertNotIn("midi.setTechniqueMappings(preset.techniqueMappings)", MAIN)
        self.assertNotIn("midi.setBreathController(preset.breathController)", MAIN)

    def test_smart_mix_separates_instrument_and_accompaniment(self):
        instrument = HOST.index("masterOutput->processInstrument")
        accompaniment = HOST.index("accompaniment->mixInto", instrument)
        master = HOST.index("masterOutput->processMaster", accompaniment)
        self.assertLess(instrument, accompaniment)
        self.assertLess(accompaniment, master)
        self.assertIn("getAccompanimentDuckGain", HOST)
        self.assertIn("instrumentReverb.processStereo", MASTER)
        self.assertIn("glueReverb.processStereo", MASTER)

    def test_transpose_is_global_and_not_part_of_sound_presets(self):
        self.assertNotIn("transposeSemitones", PRESET)
        self.assertNotIn("masterVolume", PRESET)
        commit = MAIN.split("void MainComponent::commitCurrentPreset", 1)[1].split("void MainComponent::", 1)[0]
        self.assertNotIn("transposeSemitones", commit)
        self.assertNotIn("masterVolume", commit)


if __name__ == "__main__":
    unittest.main()
