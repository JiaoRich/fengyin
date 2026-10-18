import hashlib,io,json,sys,unittest,zipfile
import xml.etree.ElementTree as ET
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'scripts'))
import disable_violin_room_123 as room
from apply_swam_gain_curve_119 import mainvolume_curve

class VlcReleaseTests(unittest.TestCase):
    def test_room_only_and_immutable_predecessor(self):
        audit=json.loads(room.AUDIT.read_text(encoding='utf-8'))
        previous=json.loads((room.ASSETS/'swam-gain-curve-119-audit.json').read_text(encoding='utf-8'))
        previous={r['file']:r for r in previous['targets']}
        self.assertEqual({r['file'] for r in audit['targets']},{'violin-soft.fytonepack','violin-bright.fytonepack'})
        with zipfile.ZipFile(room.BASELINE) as base:
            for r in audit['targets']:
                oldbytes=base.read(r['file']);newbytes=(room.ASSETS/r['file']).read_bytes()
                self.assertEqual(hashlib.sha256(oldbytes).hexdigest(),r['beforeSha256'])
                self.assertEqual(r['beforeSha256'],previous[r['file']]['afterSha256'])
                self.assertEqual(hashlib.sha256(newbytes).hexdigest(),r['afterSha256'])
                with zipfile.ZipFile(io.BytesIO(oldbytes)) as a,zipfile.ZipFile(io.BytesIO(newbytes)) as b:
                    self.assertEqual(a.namelist(),b.namelist());self.assertIsNone(b.testzip())
                    ax=ET.fromstring(a.read('tone.xml'));bx=ET.fromstring(b.read('tone.xml'))
                    ast=ax.find('PRESET/INSTRUMENT_STATE');bst=bx.find('PRESET/INSTRUMENT_STATE')
                    self.assertEqual(room.patch_room(ast.text),bst.text)
                    _,params=mainvolume_curve(bst.text)
                    self.assertEqual(next(p.get('value') for p in params.iter('PARAM') if p.get('id')=='roomEnabled'),'0.0')
                    ast.text=bst.text
                    self.assertEqual(ET.tostring(ax),ET.tostring(bx),'Changed effects, gain, mappings or snapshots')
                    for name in a.namelist():
                        if name not in ('tone.xml','manifest.json'):self.assertEqual(a.read(name),b.read(name))
                    for entry in json.loads(b.read('manifest.json'))['files']:
                        data=b.read(entry['path']);self.assertEqual(len(data),entry['bytes'])
                        self.assertEqual(hashlib.sha256(data).hexdigest(),entry['sha256'])
                if r['file']=='violin-bright.fytonepack':self.assertEqual(oldbytes,newbytes)

    def test_windows_one_player_no_extraction_or_proxy(self):
        source=(ROOT/'native/Source/MainComponent.cpp').read_text(encoding="utf-8")
        start=source.index('void MainComponent::loadVideoForWebInterface(')
        block=source[start:source.index('void MainComponent::updateIdleVideoPreparation',start)]
        vlc=block[block.index('if (fengyin::VlcVideoSurface::supported())'):block.index('if (accompaniment.loadForVideo')]
        self.assertIn('vlcVideo.player()->load',vlc);self.assertIn('return;',vlc)
        self.assertNotIn('extractAsync',vlc)
        self.assertIn('if (fengyin::VlcVideoSurface::supported()) return;',source[source.index('void MainComponent::updateIdleVideoPreparation'):])
        player=(ROOT/'native/Source/VlcPlaybackEngine.cpp').read_text(encoding="utf-8")
        self.assertIn('libvlc_media_player_set_hwnd',player)
        self.assertNotIn('libvlc_video_set_callbacks',player)
        self.assertIn('libvlc_media_parse_local',player)
        self.assertIn('request.playRevision==next.playRevision',player)

    def test_packaging_verifies_real_decoder(self):
        script=(ROOT/'scripts/build-windows.ps1').read_text(encoding='utf-8-sig')
        self.assertLess(script.index('LibVLC real playback'),script.index('主程序编译'))
        self.assertIn('components\\VLC',script)
        import prepare_vlc
        self.assertIn('videolan.libvlc.windows/',prepare_vlc.URL)
        self.assertNotIn('.gpl/',prepare_vlc.URL)
        self.assertEqual(len(prepare_vlc.SHA256),64)
        self.assertIn('sourceSha256',(ROOT/'scripts/prepare_vlc.py').read_text(encoding="utf-8"))

if __name__=='__main__':unittest.main()
