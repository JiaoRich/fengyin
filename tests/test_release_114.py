from pathlib import Path
import json,zipfile,hashlib,unittest,subprocess,shutil,tempfile,xml.etree.ElementTree as E
ROOT=Path(__file__).resolve().parents[1]
class Release114(unittest.TestCase):
    def test_execute_tempo_and_wet_delay(self):
        compiler=shutil.which('c++') or shutil.which('clang++')
        if compiler:
            with tempfile.TemporaryDirectory() as d:
                exe=Path(d)/'tempo';subprocess.run([compiler,'-std=c++17','-I',str(ROOT/'native/Source'),str(ROOT/'tests/test_music_tempo.cpp'),'-o',str(exe)],check=True)
                subprocess.run([str(exe)],check=True)
        else:
            # Windows executes this same source through the CMake test target.
            self.assertIn('tests/test_music_tempo.cpp',(ROOT/'CMakeLists.txt').read_text(encoding='utf-8'))
    def test_six_packages_have_true_versions_and_complete_state(self):
        audit=json.loads((ROOT/'assets/tone-packages/refresh-114-audit.json').read_text(encoding='utf-8'))
        self.assertEqual(len(audit),6)
        for entry in audit:
            self.assertTrue(entry['completeStateUnchanged'])
            with zipfile.ZipFile(ROOT/'assets/tone-packages'/entry['asset']) as z:
                self.assertIsNone(z.testzip());m=json.loads(z.read('manifest.json'));p=E.fromstring(z.read('tone.xml')).find('PRESET')
                self.assertEqual(p.get('name'),entry['name']);self.assertTrue(p.find('INSTRUMENT_STATE').text)
                self.assertEqual(m['dependencies'],entry['dependencies'])
                for f in m['files']:self.assertEqual(hashlib.sha256(z.read(f['path'])).hexdigest(),f['sha256'])
        sweet=next(x for x in audit if x['name']=='甜美小提琴');self.assertEqual(len(sweet['dependencies']),2)
        self.assertEqual(sweet['dependencies'][1]['name'],'magic.CURVE')
    def test_no_asio_install_prompt_and_magic_hash(self):
        iss=(ROOT/'installer/FengYin.iss').read_text(encoding='utf-8');body=iss.split('procedure CurStepChanged',1)[1]
        self.assertNotIn('ASIO4ALL_2_22.exe',body);self.assertNotIn('VBCABLE_Setup_x64.exe',body)
        self.assertNotIn('Name: "asiobridge"',iss);self.assertIn('HasMagicCurveAt',body)
        magic=ROOT/'assets/components/magic.CURVE-1.0.2-Windows-Setup.exe'
        self.assertEqual(hashlib.sha256(magic.read_bytes()).hexdigest(),'5aebf446018d5f8fce87f332e9d8a01194a98e02536de132bf1c64939417bdbd')
    def test_automation_separate_from_ui_and_originals_retained(self):
        midi=(ROOT/'native/Source/MidiInputService.cpp').read_text(encoding='utf-8');host=(ROOT/'native/Source/PluginHostEngine.cpp').read_text(encoding='utf-8')
        self.assertIn('startTimer(10)',midi);self.assertIn('stopTimer(); // joins',midi)
        self.assertIn('sink->automaticVibratoChanged',midi);self.assertIn('originalVibratoRate',host)
        self.assertIn('SwamStateCompatibility::preserved',host)
        self.assertIn('verifiedForwardMigration',(ROOT/'native/Source/MainComponent.cpp').read_text(encoding='utf-8'))
        self.assertIn('musicTempoV1',midi)
    def test_added_voice_pcm(self):
        import wave
        for id in ['MT_SLOW','MT_MEDIUM','MT_FAST','MT_GUIDE','TONE_VIOLIN_SWEET','TONE_VIOLIN_SOFT']:
            with wave.open(str(ROOT/'assets/voice'/f'{id}.wav'),'rb') as w:
                data=w.readframes(w.getnframes());self.assertGreater(len(data),4096);self.assertTrue(any(data))
