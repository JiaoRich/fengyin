import pathlib
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[1]
HOST = (ROOT / "native" / "Source" / "PluginHostEngine.cpp").read_text(encoding="utf-8")
MIDI = (ROOT / "native" / "Source" / "MidiInputService.cpp").read_text(encoding="utf-8")
MAIN = (ROOT / "native" / "Source" / "MainComponent.cpp").read_text(encoding="utf-8")
JS = (ROOT / "prototype" / "app.js").read_text(encoding="utf-8")
MASTER = (ROOT / "native" / "Source" / "MasterOutputService.cpp").read_text(encoding="utf-8")
PRESET = (ROOT / "native" / "Source" / "SoundPresetStore.cpp").read_text(encoding="utf-8")


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

    def test_transpose_releases_active_notes_and_uses_note_map(self):
        self.assertIn("void MidiInputService::setTransposeSemitones", MIDI)
        self.assertIn("activeOutputNotes", MIDI)
        self.assertIn("sourceNote + transposeSemitones.load", MIDI)
        self.assertIn("sink->noteOff(outputNote)", MIDI)

    def test_technique_learning_and_swam_parameter_control_are_connected(self):
        self.assertIn("beginTechniqueLearn", MIDI)
        self.assertIn("handleTechniqueMessage", MIDI)
        self.assertIn("resolveTechniqueParameters", HOST)
        self.assertIn('name.contains("growl")', HOST)
        self.assertIn('name.contains("flutter")', HOST)
        self.assertIn("setValueNotifyingHost", HOST)

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
