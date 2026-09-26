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
TONE_STYLES = (ROOT / "native" / "Source" / "ToneStyleCatalog.h").read_text(encoding="utf-8")
AUDIO = (ROOT / "native" / "Source" / "AudioDeviceService.cpp").read_text(encoding="utf-8")


class HostRegressionTests(unittest.TestCase):
    def test_endpoint_switch_does_not_start_or_invalidate_tuning(self):
        follow = AUDIO.split("bool AudioDeviceService::followSystemDefaultOutput()", 1)[1].split(
            "bool AudioDeviceService::systemDefaultOutputChanged()", 1)[0]
        self.assertNotIn("removeValue(\"automaticLatencyTunedDevice\")", follow)
        timer = MAIN.split("if (++audioOutputSyncTicks", 1)[1].split("if (const auto tuningResult", 1)[0]
        self.assertIn("if (! outputChanged", timer)
        self.assertIn("audioHardwareIdentity", AUDIO)

    def test_audio_tuning_stops_for_real_performance_activity(self):
        timer = MAIN.split("snapshot = midi.getSnapshot();", 1)[1].split("if (const auto tuningResult", 1)[0]
        self.assertIn("snapshot.breath > 0.01f", timer)
        self.assertIn("snapshot.lastNote >= 0", timer)
        self.assertNotIn("snapshot.breath < 4", timer)
        self.assertIn("cancelAutomaticLatencyTuning", timer)

    def test_pitch_wheel_is_not_scaled_by_saved_sensitivity(self):
        handler = MIDI.split("void MidiInputService::handleIncomingMidiMessage", 1)[1]
        pitch = handler.split("else if (message.isPitchWheel())", 1)[1].split(
            "if (message.isController()", 1)[0]
        self.assertNotIn("pitchSensitivity", pitch)
        self.assertIn("getPitchWheelValue()", pitch)

    def test_video_replacement_rejects_stale_events_and_rewinds(self):
        self.assertIn("webVideoGeneration", MAIN)
        self.assertIn("webVideoPosition = 0.0", MAIN)
        self.assertIn("generation:videoGeneration", JS)
        self.assertIn("video.currentTime = 0", JS)
        self.assertIn("videoWantsPlaying = false", JS)

    def test_custom_tone_has_independent_bass_and_direct_page(self):
        self.assertIn('toneObject->setProperty("bass", (toneSettings.bass + 1.0f)', MAIN)
        self.assertIn("result.bass =", MAIN)
        self.assertIn("定制音色", JS)
        self.assertIn("enterCustomToneCreate", JS)
        self.assertIn("custom-tone-empty", (ROOT / "prototype" / "index.html").read_text(encoding="utf-8"))

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

    def test_breath_is_raw_passthrough_and_precedes_note_on(self):
        self.assertIn("sink->resetPerformance()", MIDI)
        note_block = MIDI.split("if (message.isNoteOn())", 1)[1].split("else if (message.isNoteOff())", 1)[0]
        self.assertLess(note_block.index("sink->breathChanged(currentBreath"), note_block.index("sink->noteOn"))
        self.assertIn("const auto rawBreath = static_cast<float>(message.getControllerValue()) / 127.0f", MIDI)
        self.assertNotIn("processMidiValue", MIDI)
        self.assertNotIn("0.28f + currentBreath * 0.52f", MIDI)
        self.assertIn('activeProfile.id == "yamaha-yds"', MIDI)
        self.assertIn("savedController = 11", MIDI)
        self.assertIn("juce::MidiMessage::allNotesOff", HOST)
        self.assertIn("controllerEvent(1, 11, 0)", HOST)
        self.assertIn("swamExpressionController.load", HOST)
        self.assertIn("applyStandardSwamExpressionCurve", MAIN)

    def test_swam_main_expression_curve_preserves_controller_mapping(self):
        curve = (ROOT / "native" / "Source" / "SwamExpressionCurve.cpp").read_text(encoding="utf-8")
        header = (ROOT / "native" / "Source" / "SwamExpressionCurve.h").read_text(encoding="utf-8")
        self.assertIn('parameterId").equalsIgnoreCase("expression")', curve)
        self.assertIn('result.controller = element.getIntAttribute("msb", -1)', curve)
        self.assertNotIn('setAttribute("msb"', curve)
        self.assertNotIn('setAttribute("channel"', curve)
        self.assertIn("outputMaximum = 116.0", header)
        self.assertIn("shape = 0.10", header)

    def test_transpose_releases_active_notes_and_uses_note_map(self):
        self.assertIn("void MidiInputService::setTransposeSemitones", MIDI)
        self.assertIn("transposeFromTo", PITCH_KEY)
        self.assertIn("PitchKey::transposeFromTo", MIDI)
        self.assertIn("PitchKey::transposeFromTo(0", MIDI)
        self.assertNotIn("keyCalibrationPending", MIDI)
        self.assertNotIn("sourceKey", MIDI)
        self.assertIn("activeOutputNotes", MIDI)
        self.assertIn("sourceNote + transposeSemitones.load", MIDI)
        self.assertIn("sink->noteOff(outputNote)", MIDI)

    def test_machine_code_copy_and_c_key_transpose_warning_exist(self):
        self.assertIn('id="copy-machine-code"', (ROOT / "prototype" / "index.html").read_text(encoding="utf-8"))
        self.assertIn("nativeEvent('copyMachineCode')", JS)
        html = (ROOT / "prototype" / "index.html").read_text(encoding="utf-8")
        self.assertIn('id="transpose-dialog"', html)
        self.assertIn("请先将电吹管上的调值设置为 C 调", html)
        self.assertNotIn("beginKeyCalibration", MAIN)
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
        self.assertIn("isWindowsSharedType(current->getTypeName())", audio_device)
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
        self.assertIn("sharedMixRate(*device)", audio_device)
        self.assertIn("minimumLegalBuffer", audio_device)
        self.assertNotIn("recommendedInitialBuffer", audio_device)
        self.assertIn("audio.applyBestInitialSetup();", MAIN)
        self.assertIn('withEventListener("applyAudioSettings"', MAIN)
        self.assertIn('withEventListener("requestAudioSettings"', MAIN)
        self.assertIn('emitEventIfBrowserIsVisible("audioSettingsState"', MAIN)

    def test_audio_setup_uses_real_driver_capabilities_and_runtime_stability(self):
        audio_device = (ROOT / "native" / "Source" / "AudioDeviceService.cpp").read_text(encoding="utf-8")
        self.assertIn("sortedLegalBuffers", audio_device)
        self.assertIn("minimumLegalBuffer", audio_device)
        self.assertIn("getAvailableBufferSizes()", audio_device)
        self.assertIn("manager.getCpuUsage() >= 0.82", audio_device)
        self.assertIn("audio.hasSustainedRuntimeInstability()", MAIN)
        self.assertNotIn("stabiliseAfterXRuns", audio_device)
        self.assertNotIn("candidate <= 1024", audio_device)

    def test_first_run_audio_tuning_is_automatic_but_manual_controls_remain(self):
        audio_header = (ROOT / "native" / "Source" / "AudioDeviceService.h").read_text(encoding="utf-8")
        audio_device = (ROOT / "native" / "Source" / "AudioDeviceService.cpp").read_text(encoding="utf-8")
        self.assertIn("beginAutomaticLatencyTuning", audio_header)
        self.assertIn("pollAutomaticLatencyTuning", audio_header)
        self.assertIn("latencyCandidateTestMs", audio_device)
        self.assertIn("isWindowsSharedType", audio_device)
        self.assertIn("Test only periods the active IAudioClient3 endpoint explicitly reports", audio_device)
        self.assertIn("addedXRuns == 0", audio_device)
        self.assertIn("tuningCandidateMaximumCpu < 0.65", audio_device)
        self.assertIn("tuningCandidateMaximumCpu", audio_device)
        self.assertIn('automaticLatencyTunedDevice', audio_device)
        self.assertIn("pluginHost.setLatencyProbeActive(true)", MAIN)
        self.assertIn("pluginHost.setLatencyProbeActive(false)", MAIN)
        constructor = MAIN.split("MainComponent::MainComponent", 1)[1].split("MainComponent::~MainComponent", 1)[0]
        self.assertNotIn("audio.beginAutomaticLatencyTuning();", constructor)
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

    def test_inline_breath_match_is_persisted_without_a_modal(self):
        self.assertIn('withEventListener("beginBreathMatch"', MAIN)
        self.assertIn('"breathMatchResult"', MAIN)
        self.assertIn("midi.finishBreathDetection(true)", MAIN)
        self.assertIn('state->setProperty("deviceMatched"', MAIN)
        self.assertIn("midi.hasCompletedBreathMatch()", MAIN)
        self.assertIn('controllerSettingKey() + ".matched"', MIDI)
        self.assertIn("bool markCompleted", MIDI)
        self.assertIn("webBreathDetectionActive", MAIN)

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

    def test_intelligent_techniques_use_global_roles_and_drive_real_parameters(self):
        processor = (ROOT / "native" / "Source" / "IntelligentTechniqueProcessor.h").read_text(encoding="utf-8")
        advisor = (ROOT / "native" / "Source" / "TechniqueAdvisor.h").read_text(encoding="utf-8")
        self.assertIn("setTechniqueConfiguration", MAIN)
        self.assertIn("updateBreathDrivenTechniques", MIDI)
        self.assertIn("onsetGuardMs", processor)
        self.assertIn("PerformanceTechnique::vibrato", processor)
        self.assertIn('device.id == "yamaha-yds"', advisor)
        self.assertIn("TechniqueControlMode::breath", advisor)
        self.assertIn("growlOnThreshold", processor)
        self.assertIn("113.0f / 127.0f", processor)
        self.assertIn("118.0f / 127.0f", processor)
        self.assertIn("123.0f / 127.0f", processor)
        self.assertIn("80.0, 0.0, 85.0f, 110.0f", processor)
        self.assertIn('return "technique.roles.v2."', MIDI)
        self.assertIn("targetForRole", MIDI)

    def test_kong_and_swam_are_supported_while_other_plugins_are_blocked(self):
        self.assertIn('item->setProperty("supported", isSwam || isKong)', MAIN)
        self.assertIn('withEventListener("loadKongInstrument"', MAIN)
        self.assertIn('pluginHost.selectProgramByAliases', MAIN)
        self.assertIn("当前版本尚未支持 Kontakt、三体等其他音源", MAIN)
        self.assertIn("SwamFamily::notSwam", MAIN)
        self.assertIn('return "technique.roles.v2."', MIDI)
        self.assertIn('name.contains("portamento")', HOST)
        self.assertIn('name.contains("bowpressure")', HOST)

    def test_smart_mix_separates_instrument_and_accompaniment(self):
        instrument = HOST.index("masterOutput->processInstrument")
        accompaniment = HOST.index("accompaniment->mixInto", instrument)
        master = HOST.index("masterOutput->processMaster", accompaniment)
        self.assertLess(instrument, accompaniment)
        self.assertLess(accompaniment, master)
        self.assertIn("instrumentReverb.processStereo", MASTER)
        self.assertNotIn("glueReverb", MASTER)
        self.assertIn("不得修改伴奏原声", MASTER)

    def test_realtime_midi_preserves_timestamps_and_avoids_cross_thread_collector_writes(self):
        header = (ROOT / "native" / "Source" / "PluginHostEngine.h").read_text(encoding="utf-8")
        queue = (ROOT / "native" / "Source" / "RealtimeMidiQueue.h").read_text(encoding="utf-8")
        self.assertIn("midiQueueSize = 8192", header)
        self.assertIn("RealtimeMidiQueue<midiQueueSize>", header)
        self.assertIn("timestampSeconds", queue)
        self.assertIn("writeIndex", queue)
        self.assertIn("readIndex", queue)
        self.assertNotIn("mutex", queue.lower())
        self.assertIn("message.getTimeStamp()", MIDI)
        self.assertIn("player.enqueueMidi(message, timestampSeconds)", HOST)
        self.assertIn("count < midiQueueSize && midiQueue.pop(event)", HOST)
        self.assertIn("timestampSeconds > 0.0 ? midiQueue : controlQueue", HOST)
        self.assertIn("ensureStorageAllocated", HOST)
        queue_body = HOST.split("void PluginHostEngine::queue", 1)[1]
        self.assertNotIn("getMidiMessageCollector().addMessageToQueue", queue_body)

    def test_transpose_is_global_and_not_part_of_sound_presets(self):
        self.assertNotIn("transposeSemitones", PRESET)
        self.assertNotIn("masterVolume", PRESET)
        commit = MAIN.split("void MainComponent::commitCurrentPreset", 1)[1].split("void MainComponent::", 1)[0]
        self.assertNotIn("transposeSemitones", commit)
        self.assertNotIn("masterVolume", commit)

    def test_tone_styles_drive_builtin_zero_latency_chain_and_roundtrip(self):
        self.assertIn('withEventListener("setToneStyle"', MAIN)
        self.assertIn("masterOutput.setToneStyle", MAIN)
        self.assertIn('child->setAttribute("toneStyleId"', PRESET)
        self.assertIn('child->setAttribute("warmth"', PRESET)
        self.assertIn("styleCompressionThreshold", MASTER)
        self.assertIn("styleHarshControl", MASTER)
        self.assertIn("std::tanh", MASTER)
        self.assertIn('"silky"', TONE_STYLES)
        self.assertIn('"warm-jazz"', TONE_STYLES)
        self.assertIn('"cinematic"', TONE_STYLES)
        self.assertNotIn("favoritePresetButton", MAIN)

    def test_swam_models_and_tone_profiles_are_real_but_never_touch_midi_mapping(self):
        header = (ROOT / "native" / "Source" / "PluginHostEngine.h").read_text(encoding="utf-8")
        self.assertIn("getInstrumentModelNames", header)
        self.assertIn("selectInstrumentModel", header)
        self.assertIn('withEventListener("setInstrumentModel"', MAIN)
        self.assertIn('state->setProperty("instrumentModels"', MAIN)
        self.assertIn("applySwamToneProfile", HOST)
        self.assertIn("isProtectedPerformanceParameter", HOST)
        for protected in ("midi", "controller", "breath", "expression", "pitchbend", "growl", "vibrato"):
            self.assertIn(f'"{protected}"', HOST)
        self.assertIn("parameter == instrumentModelParameter", HOST)
        for key in ("soprano-sax", "alto-sax", "tenor-sax", "baritone-sax"):
            block = TONE_STYLES.split(f'key == "{key}"', 1)[1].split("if (key ==", 1)[0]
            self.assertGreaterEqual(block.count("{ true,"), 3)

    def test_custom_tone_settings_preserve_builtin_styles(self):
        self.assertIn('withEventListener("previewCustomTone"', MAIN)
        self.assertIn('withEventListener("cancelCustomTone"', MAIN)
        self.assertIn('withEventListener("saveCustomPreset"', MAIN)
        self.assertIn('preset.customTone = true', MAIN)
        self.assertIn('child->setAttribute("customTone"', PRESET)


if __name__ == "__main__":
    unittest.main()
