from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]
MAIN = (ROOT / 'native/Source/MainComponent.cpp').read_text(encoding='utf-8')
MIDI = (ROOT / 'native/Source/MidiInputService.cpp').read_text(encoding='utf-8')


class MidiReconnectTests(unittest.TestCase):
    def test_disconnect_discards_partial_match(self):
        disconnect = MIDI.split('void MidiInputService::disconnect()', 1)[1].split('void MidiInputService::refreshAndConnectFirstAvailable', 1)[0]
        self.assertIn('cancelBreathDetection();', disconnect)
        self.assertIn('++connectionGeneration;', disconnect)
        self.assertNotIn('finishBreathDetection', disconnect)

    def test_only_an_active_explicit_match_can_commit(self):
        finish = MIDI.split('int MidiInputService::finishBreathDetection', 1)[1].split('void MidiInputService::setMusicTempoMode', 1)[0]
        self.assertIn('detectingBreath.exchange(false', finish)
        self.assertIn('if (!wasDetecting || !markCompleted)', finish)
        self.assertLess(finish.index('return -1;'), finish.index('savePreferences();'))

    def test_restore_and_ui_connection_generation_are_wired(self):
        connect = MIDI.split('bool MidiInputService::connect(', 1)[1].split('void MidiInputService::disconnect()', 1)[0]
        self.assertIn('restoreBreathController();', connect)
        self.assertIn('loadTechniqueMappings();', connect)
        self.assertNotIn('beginBreathDetection', connect)
        self.assertIn('midiConnectionGeneration != breathDetectionConnectionGeneration', MAIN)
        self.assertIn('breathDetectionConnectionGeneration = midi.getConnectionGeneration()', MAIN)


if __name__ == '__main__':
    unittest.main()
