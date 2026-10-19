from pathlib import Path
import unittest

ROOT=Path(__file__).resolve().parents[1]

class RealtimeRenderBudget(unittest.TestCase):
    def test_native_dashboard_does_not_invalidate_browser_children(self):
        source=(ROOT/'native/Source/MainComponent.cpp').read_text(encoding='utf-8')
        paint=source.split('void MainComponent::paint(',1)[1].split('void MainComponent::resized(',1)[0]
        self.assertIn('if (webInterface != nullptr && webInterfaceReady) return;',paint)
        timer=source.split('void MainComponent::timerCallback()',1)[1].split('void MainComponent::emitSuperLowLatencyState',1)[0]
        self.assertIn('if (webInterface == nullptr || !webInterfaceReady) repaint();',timer)

    def test_only_static_host_uses_software_not_video_or_plugins(self):
        helper=(ROOT/'native/Source/VideoHostRendering.h').read_text(encoding='utf-8')
        self.assertIn('engines.indexOf("Software Renderer")',helper)
        self.assertIn('peer->getCurrentRenderingEngine() == software',helper)
        main=(ROOT/'native/Source/Main.cpp').read_text(encoding='utf-8')
        diagnostic=(ROOT/'tools/vlc-webview-check/main.cpp').read_text(encoding='utf-8')
        self.assertIn('fengyin::configureVideoHostRendering(getPeer())',main)
        self.assertIn('fengyin::configureVideoHostRendering(getPeer())',diagnostic)
        self.assertNotIn('disable-gpu',main)
        self.assertNotIn('VideoHostRendering', (ROOT/'native/Source/PluginHostEngine.cpp').read_text(encoding='utf-8'))

    def test_meter_events_and_decorations_have_separate_budget(self):
        js=(ROOT/'prototype/app.js').read_text(encoding='utf-8')
        self.assertIn('if (!backendControlsChanged(state)) return;',js)
        self.assertIn('realtime ? 1000/12 : 1000/24',js)
        self.assertIn("if (!realtime && $('#page-play').classList.contains('active')) drawBreathWave",js)
        css=(ROOT/'prototype/styles.css').read_text(encoding='utf-8')
        self.assertIn('body.realtime-rendering',css)
        self.assertIn('backdrop-filter:none!important',css)

    def test_native_meter_packets_do_not_serialize_catalogues_or_compute_unused_fft(self):
        source=(ROOT/'native/Source/MainComponent.cpp').read_text(encoding='utf-8')
        timer=source.split('void MainComponent::timerCallback()',1)[1].split('void MainComponent::emitSuperLowLatencyState',1)[0]
        self.assertIn('webNow - lastWebStateAtMs >= 1000.0',timer)
        meters=timer.split('auto meters=std::make_unique<juce::DynamicObject>();',1)[1].split('"backendMeterState"',1)[0]
        for catalogue in ('publishedTones','getKongLibraryState','cachedPresets','instrumentModels','describeEffectChain'):
            self.assertNotIn(catalogue,meters)
        self.assertNotIn('masterOutput.getSpectrum(',timer)
        self.assertIn('.withEventListener("requestBackendState"',source)
        js=(ROOT/'prototype/app.js').read_text(encoding='utf-8')
        self.assertIn("addEventListener('backendMeterState'",js)
        self.assertIn("backend.emitEvent('requestBackendState',{})",js)

if __name__=='__main__':unittest.main()
