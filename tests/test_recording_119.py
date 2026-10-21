from pathlib import Path
import unittest
ROOT=Path(__file__).resolve().parents[1]
class Recording119(unittest.TestCase):
    def test_root_and_two_subfolders_and_two_choices(self):
        paths=(ROOT/'native/Source/RecordingPaths.h').read_text(encoding='utf-8')
        for text in ['风吟录制','录音文件','录屏文件','临时文件']:self.assertIn(text,paths)
        html=(ROOT/'prototype/index.html').read_text(encoding='utf-8')
        self.assertIn('我的录制',html)
        self.assertIn('id="record-menu"',html)
        self.assertIn('● 开始录制',html)
        js=(ROOT/'prototype/app.js').read_text(encoding='utf-8')
        self.assertNotIn('showScreenRecordings',js)
        self.assertIn("$('#record').addEventListener('click',()=>",js)
    def test_capture_thread_is_independent_of_encoder(self):
        source=(ROOT/'native/ScreenRecorder/main.cpp').read_text(encoding='utf-8')
        thread=source.split('worker=std::thread',1)[1].split('future.get()',1)[0]
        self.assertIn('Loopback loop;loop.open()',thread)
        self.assertNotIn('writer.',thread)
        self.assertIn('queuedFrames>48000*10',source)
        self.assertIn('sound.stop();sound.drain(writer)',source)
        self.assertNotIn('sound.silence(writer',source)
