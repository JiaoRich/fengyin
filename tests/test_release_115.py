import hashlib,json,unittest,wave,subprocess,shutil
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
class Release115(unittest.TestCase):
    def test_reviewed_voice_assets_match_metadata(self):
        audit_file=ROOT/'assets/voice/release-117-audit.json'
        if not audit_file.exists():audit_file=ROOT/'assets/voice/release-115-audit.json'
        audit=json.loads(audit_file.read_text(encoding='utf-8'))
        catalog=json.loads((ROOT/'prototype/voice-catalog.js').read_text(encoding='utf-8').split('=',1)[1].strip().rstrip(';'))
        self.assertEqual(len(audit),123 if audit_file.name in ('release-116-audit.json','release-117-audit.json') else 122)
        self.assertEqual({x['id'] for x in audit},{x['id'] for x in catalog['items']})
        self.assertEqual(catalog['voice'],'x6_wuyediantai_mini')
        for row in audit:
            file=ROOT/'assets/voice'/(row['id']+'.wav')
            self.assertEqual(hashlib.sha256(file.read_bytes()).hexdigest(),row['sha256'])
            with wave.open(str(file)) as w:self.assertGreater(w.getnframes(),1000)
        self.assertEqual(next(x['text'] for x in audit if x['id']=='TONE_VIOLIN_BRIGHT'),'小提琴，明亮。')
        self.assertTrue({'TONE_DONGXIAO','TONE_BANGDI','TONE_JINGHU'}.issubset({x['id'] for x in audit}))
    def test_policy_and_missing_mapping_default_execute(self):
        node=shutil.which('node')
        if node is None:node='/Users/mengyang.jmy/.cache/codex-runtimes/codex-primary-runtime/dependencies/node/bin/node'
        source=(ROOT/'prototype/app.js').read_text(encoding='utf-8')
        functions=source[source.index('function buildTechniquePlan('):source.index('function buildAdaptiveTechniquePlan(')]
        functions+=source[source.index('function mergeBackendTechniquePlan('):source.index('function inferInstrumentKey(')]
        harness="const currentInstrument=null;"+functions+"""
const assert=require('assert');
for(const key of ['flute','piccolo','clarinet','oboe','trumpet','bass-trombone','violin','kong-erhu-2','kong-jinghu']) {
 const vib=mergeBackendTechniquePlan(key,{},[]).find(x=>x.id==='vibrato');
 assert(vib.pluginSupported,key);assert.equal(vib.mode,'breath');
 assert.equal(mergeBackendTechniquePlan(key,{},[{id:'vibrato',mode:4}])[0].mode,'off');
}
assert(!buildTechniquePlan('kong-pipa')[0].pluginSupported);
"""
        subprocess.run([node,'-e',harness],check=True)
        self.assertIn('class="clear-source"',source)
        self.assertIn("nativeEvent('removeTechniqueMapping',{technique:item.index})",source)
        backend=(ROOT/'native/Source/MainComponent.cpp').read_text(encoding='utf-8').split('.withEventListener("removeTechniqueMapping"',1)[1].split('.withEventListener',1)[0]
        self.assertIn('TechniqueSourceType::none',backend)
        self.assertIn('TechniqueControlMode::off',backend)
        self.assertIn('midi.cancelTechniqueLearn()',backend)
        host=(ROOT/'native/Source/PluginHostEngine.cpp').read_text(encoding='utf-8')
        profile=host.split('void PluginHostEngine::configureKongTechniqueProfile',1)[1].split('void PluginHostEngine::resetPerformance',1)[0]
        self.assertIn('if(simpleVibrato(key.toStdString()))',profile)
        self.assertNotIn('bind(PerformanceTechnique::vibrato',profile)
if __name__=='__main__':unittest.main()
