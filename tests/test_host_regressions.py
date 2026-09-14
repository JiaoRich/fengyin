import pathlib
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[1]
HOST = (ROOT / "native" / "Source" / "PluginHostEngine.cpp").read_text(encoding="utf-8")
MIDI = (ROOT / "native" / "Source" / "MidiInputService.cpp").read_text(encoding="utf-8")
MAIN = (ROOT / "native" / "Source" / "MainComponent.cpp").read_text(encoding="utf-8")
JS = (ROOT / "prototype" / "app.js").read_text(encoding="utf-8")


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


if __name__ == "__main__":
    unittest.main()
