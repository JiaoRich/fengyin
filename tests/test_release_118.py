from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]

class Release118(unittest.TestCase):
    def read(self, name):
        return (ROOT / name).read_text(encoding='utf-8')

    def test_installer_character_arguments(self):
        text = self.read('installer/FengYin.iss')
        self.assertIn("GetDateTimeString('yyyymmddhhnnss', #0, #0)", text)
        self.assertNotIn("GetDateTimeString('yyyymmddhhnnss', '', '')", text)
        self.assertIn('AlwaysRestart=yes', text)

    def test_startup_incremental_and_description_survives_validation(self):
        main = self.read('native/Source/MainComponent.cpp')
        self.assertIn('pluginCatalog.startScan(pluginCatalog.getRecommendedVst3Paths(), pluginCatalog.needsDependencyRefresh())', main)
        worker = self.read('native/Source/PluginScanWorker.h')
        self.assertLess(worker.index('document.writeTo(output)'), worker.index('next();'))
        self.assertNotIn('setApplicationReturnValue(4)', worker)
        scanner = self.read('native/Source/PluginCatalogService.cpp')
        self.assertIn('knownPlugins.removeFromBlacklist(path)', scanner)
        self.assertIn('return !result.isEmpty()', scanner)

    def test_breath_changes_are_deferred(self):
        self.assertNotIn('setBreathResponseMode', self.read('native/Source/MainComponent.cpp'))
        self.assertNotIn('breath-response-mode', self.read('prototype/app.js'))
        self.assertIn('.85f, .95f', self.read('native/Source/BreathResponse.h'))

    def test_exit_recovery_is_bounded(self):
        router = self.read('audio-engine/windows/DefaultEndpointRouter.cpp')
        self.assertIn('attempt < 3', router)
        self.assertIn('getId(*enumerator.Get(), roles[index]) == target', router)
        watcher = self.read('audio-engine/windows/AudioWatchdogMain.cpp')
        self.assertIn('attempt < 10', watcher)
        self.assertIn('Exit recovery deferred to new engine', watcher)
