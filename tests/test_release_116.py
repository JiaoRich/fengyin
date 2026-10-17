import hashlib,json,unittest,zipfile,xml.etree.ElementTree as ET
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
class Release116(unittest.TestCase):
    def test_kensa_state_and_effect_chain_are_preserved(self):
        def read(path):
            with zipfile.ZipFile(path) as z:
                self.assertIsNone(z.testzip())
                return ET.fromstring(z.read('tone.xml')).find('PRESET')
        # The received 118 revision adds the user-assigned bend automation.
        # Compare against that unmodified incoming source, not the retired state.
        old=read(ROOT/'assets/incoming-tones/20261017/Kontakt音源-肯萨-弯音绑定.fytonepack')
        new=read(ROOT/'assets/tone-packages/kontakt-kensa.fytonepack')
        self.assertEqual(new.get('instrumentKey'),'kontakt-kensa')
        self.assertEqual([ET.tostring(x) for x in old],[ET.tostring(x) for x in new])
        for key,value in old.attrib.items():
            if key not in ['instrumentKey','instrumentChineseName']:self.assertEqual(value,new.get(key))
    def test_visibility_gate_does_not_stop_business_timer(self):
        main=(ROOT/'native/Source/MainComponent.cpp').read_text(encoding='utf-8')
        gate=main.index('if (webPresentationVisible &&')
        self.assertGreater(main.index('masterOutput.getSpectrum',gate),gate)
        self.assertLess(main.index('// Automatic technique timing'),gate)
        self.assertIn('lastWebStateAtMs = 0',main)
        visibility=main.split('void MainComponent::setWebPresentationVisible',1)[1].split('void MainComponent::setupWebInterface',1)[0]
        for unsafe in ['stopTimer','webInterface.reset','suspendProcessing','pause()']:self.assertNotIn(unsafe,visibility)
    def test_suona_is_opt_in_and_kontakt_does_not_guess_frequency(self):
        host=(ROOT/'native/Source/PluginHostEngine.cpp').read_text(encoding='utf-8')
        self.assertIn('FENGYIN_SUONA_LEGACY',host)
        self.assertIn('slots.getNumChildren()==2',host)
        self.assertIn('suonaDualVoice.setFlutter(safeValue>.5f)',host)
        self.assertIn('controllerEvent(1,1,midiValue)',host)
        # 1.1.18 adds a Kensa-only rate conversion derived from the supplied
        # script; arbitrary Kontakt libraries must still not receive this CC.
        rate=host.split('if(tempoRateDirty.exchange(false))',1)[1].split('for (size_t index',1)[0]
        self.assertIn('if(kontaktStandardControls.load())',rate)
        self.assertIn('controllerEvent(1,19,kensaVibratoRateController(hz))',rate)
    def test_only_four_folk_defaults_changed(self):
        audit=json.loads((ROOT/'assets/tone-packages/folk-defaults-116-audit.json').read_text(encoding='utf-8'))
        changed={'folk-'+x+'.fytonepack' for x in audit['targets']}
        self.assertEqual(len(changed),4)
        gain_audit=json.loads((ROOT/'assets/tone-packages/output-gain-117-audit.json').read_text(encoding='utf-8'))
        gain_rows={row['file']:row for row in gain_audit['targets']}
        for name,digest in audit['after'].items():
            row=gain_rows.get(name)
            if row:self.assertEqual(row['beforePackageSha256'],digest)
            expected=row['afterPackageSha256'] if row else digest
            self.assertEqual(hashlib.sha256((ROOT/'assets/tone-packages'/name).read_bytes()).hexdigest(),expected)
            if name not in changed:self.assertEqual(audit['before'][name],digest)
    def test_container_reuse_is_bounded_and_does_not_keep_effects(self):
        host=(ROOT/'native/Source/PluginHostEngine.cpp').read_text(encoding='utf-8')
        park=host.split('void PluginHostEngine::parkCurrentContainer()',1)[1].split('void PluginHostEngine::prewarmContainers',1)[0]
        self.assertIn('Brand::kong',park);self.assertIn('Brand::kontakt',park)
        self.assertIn('containerCache.size() >= 2',park)
        self.assertIn('player.setProcessor(nullptr)',park)
        self.assertIn('graph->removeNode(effect.node->nodeID)',park)
        self.assertIn('containerWarmWaiters[cacheKey] = std::move(finish)',host)
        self.assertIn('restoredProcessor->suspendProcessing(false)',host)
    def test_prewarm_waits_for_idle_scan_and_never_opens_editor(self):
        main=(ROOT/'native/Source/MainComponent.cpp').read_text(encoding='utf-8')
        gate=main.split('if (!containerPrewarmStarted',1)[1].split('if (snapshot.breath',1)[0]
        for condition in ['isActivated','pluginChoicesLoaded','!pluginCatalog.getProgress().scanning','!pluginLoading','!effectLoading','!recorder.isRecording()','snapshot.breath <= 0.01f','snapshot.lastNote < 0']:
            self.assertIn(condition,gate)
        host=(ROOT/'native/Source/PluginHostEngine.cpp').read_text(encoding='utf-8')
        warm=host.split('void PluginHostEngine::prewarmContainers',1)[1].split('void PluginHostEngine::unload()',1)[0]
        self.assertNotIn('showPluginEditor',warm)
        self.assertNotIn('setStateInformation',warm)
    def test_current_voice_assets_have_traceable_exact_synthesis_text(self):
        audit=json.loads((ROOT/'assets/voice/release-117-audit.json').read_text(encoding='utf-8'))
        self.assertEqual(len(audit),123)
        byid={x['id']:x for x in audit}
        self.assertEqual(byid['V014_26']['speechText'],'二胡')
        self.assertEqual(byid['NEW_TONE_GENERIC']['speechText'],'音色加载完成。')
        self.assertEqual(byid['V058_01']['speechText'],'颤音控制已关闭。')
        for x in audit:
            self.assertEqual(x['voice'],'x6_wuyediantai_mini')
            self.assertEqual(hashlib.sha256((ROOT/'assets/voice'/(x['id']+'.wav')).read_bytes()).hexdigest(),x['sha256'])
if __name__=='__main__':unittest.main()
