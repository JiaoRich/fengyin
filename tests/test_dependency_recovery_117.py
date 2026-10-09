from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
def read(path):
    return (ROOT / path).read_text(encoding='utf-8-sig')

class DependencyRecoveryTests(unittest.TestCase):
    def test_role_policy_execution(self):
        compiler = shutil.which('clang++') or shutil.which('g++')
        self.assertIsNotNone(compiler)
        source = r'''
#include "audio-engine/common/RouteRecoveryPolicy.h"
#include <cassert>
using namespace fengyin::audioengine;
int main() {
 assert(recoveryRoleAction(L"speaker", L"cable", L"cable") == RecoveryRoleAction::restore);
 assert(recoveryRoleAction(L"speaker", L"headphones", L"cable") == RecoveryRoleAction::preserve);
 assert(recoveryRoleAction(L"speaker", L"user-virtual", L"cable") == RecoveryRoleAction::preserve);
 assert(recoveryRoleAction(L"speaker", L"", L"cable") == RecoveryRoleAction::retry);
 assert(recoveryRoleAction(L"@preserve", L"cable", L"cable") == RecoveryRoleAction::preserve);
 assert(recoveryRoleAction(L"speaker", L"speaker", L"cable") == RecoveryRoleAction::preserve);
 assert(recoveryRoleAction(L"speaker", L"cable", L"") == RecoveryRoleAction::preserve);
}
'''
        with tempfile.TemporaryDirectory() as folder:
            binary = Path(folder) / 'test'
            subprocess.run([compiler, '-std=c++17', '-x', 'c++', '-', '-I', str(ROOT), '-o', str(binary)],
                           input=source, text=True, check=True, capture_output=True)
            subprocess.run([str(binary)], check=True)

    def test_shutdown_does_not_mutate_on_query(self):
        source = read('audio-engine/windows/AudioEngineMain.cpp')
        self.assertIn('if (message == WM_QUERYENDSESSION) return TRUE;', source)
        self.assertIn('if (message == WM_ENDSESSION && w)', source)
        self.assertIn('SessionEndRecovery sessionRecovery(router)', source)

    def test_recovery_is_bounded_and_ownership_locked(self):
        source = read('audio-engine/windows/DefaultEndpointRouter.cpp')
        self.assertIn('RouteOwnership lock;', source)
        self.assertIn('ownership = std::make_unique<RouteOwnership>()', source)
        self.assertIn('MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH', source)
        self.assertIn('getId(*enumerator.Get(), roles[index]) == target', source)
        self.assertIn('attempt < 30', read('audio-engine/windows/AudioWatchdogMain.cpp'))

    def test_installer_and_scan_generation(self):
        source = read('installer/FengYin.iss')
        self.assertIn('ExecAsOriginalUser', source)
        self.assertIn('--snapshot-install', source)
        self.assertIn('--login-recovery', source)
        self.assertIn('AlwaysRestart=yes', source)
        self.assertNotIn('postinstall skipifsilent', source)
        self.assertNotIn('Check: NeedsASIO4ALL', source)
        self.assertNotIn('Check: NeedsFreshAir', source)
        scanner = read('native/Source/PluginCatalogService.cpp')
        self.assertIn('scanner->getFailedFiles().isEmpty()', scanner)
        self.assertIn('dependencies-scanned.txt', scanner)
        self.assertIn('pluginCatalog.needsDependencyRefresh()', read('native/Source/MainComponent.cpp'))

if __name__ == '__main__': unittest.main()
