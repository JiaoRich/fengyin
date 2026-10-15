import json
import unittest
import wave
import array
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

class TransposePresentationTests(unittest.TestCase):
    def test_single_stepper_and_no_duplicate_video_button(self):
        html = (ROOT/'prototype/index.html').read_text(encoding="utf-8")
        js = (ROOT/'prototype/app.js').read_text(encoding="utf-8")
        self.assertNotIn('main-play', html)
        self.assertNotIn('main-play', js)
        self.assertIn('id="performance-key"', html)
        self.assertIn('电吹管需设C调', html)
        self.assertNotIn(' 半音 · 0跟随电吹管', html)
        self.assertIn('function changePerformanceKey', js)

    def test_same_voice_and_short_announcements(self):
        raw = (ROOT/'prototype/voice-catalog.js').read_text(encoding="utf-8")
        catalog = json.loads(raw.split('=',1)[1].strip().rstrip(';'))
        items = {i['id']:i for i in catalog['items']}
        self.assertEqual(items['V026_01']['text'], '演奏调已设为C调。')
        self.assertEqual(items['V026_12']['text'], '演奏调已设为B调。')
        self.assertEqual(catalog['voice'],'x6_wuyediantai_mini')
        for item in items.values():
            self.assertEqual(item.get('voice',catalog['voice']), catalog['voice'])
            self.assertGreater((ROOT/'assets/voice'/f"{item['id']}.wav").stat().st_size,4096)
            with wave.open(str(ROOT/'assets/voice'/f"{item['id']}.wav")) as wav:
                self.assertEqual(wav.getsampwidth(),2)
                pcm = array.array('h',wav.readframes(wav.getnframes()))
                self.assertGreater(len(pcm),1000)
                self.assertGreater(max(map(abs,pcm)),100)
