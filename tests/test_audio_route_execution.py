"""Execute the router's actual virtual-device predicate against the field report."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class RouteExecution(unittest.TestCase):
    def test_endpoint_matching_records_each_pre_enumeration_failure(self):
        source = (ROOT / 'audio-engine/windows/AsioEndpointSelection.h').read_text(encoding='utf-8')
        for stage in ('null driver', 'empty requested endpoint',
                      'CoCreateInstance(MMDeviceEnumerator)', 'GetDevice(requested endpoint)',
                      'Activate(IDeviceTopology)', 'GetConnector(0)', 'GetConnectedTo(adapter)',
                      'QueryInterface(IPart)', 'QueryInterface(ASIO4ALL private API)',
                      'no interface path match', 'enumeration callback was not invoked',
                      'getPin(flow)', 'getPin(channels)', 'getPin(flags)'):
            self.assertIn(stage, source)
        self.assertIn('ASIO requested endpoint=', source)
        self.assertIn('ASIO resolved stream=', source)
        self.assertIn('HRESULT=0x', source)

    def test_reported_virtual_devices_are_never_physical_restore_targets(self):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C++ compiler unavailable')
        source = (ROOT / 'audio-engine/windows/DefaultEndpointRouter.cpp').read_text(encoding='utf-8')
        predicate = source.split('bool isVirtual(', 1)[1].split('\nstd::wstring getId', 1)[0]
        program = r'''
#include <string>
#include <algorithm>
#include <cwctype>
#include <cassert>
std::wstring lower(std::wstring s) {
    std::transform(s.begin(),s.end(),s.begin(),[](wchar_t c){return std::towlower(c);});return s;
}
''' + 'bool isVirtual(' + predicate + r'''
int main() {
    assert(isVirtual(L"",L"VB-Audio Point 1"));
    assert(isVirtual(L"",L"CABLE In 16ch (VB-Audio Virtual Cable)"));
    assert(isVirtual(L"",L"CABLE In 16ch"));
    assert(isVirtual(L"",L"CABLE Input"));
    assert(isVirtual(L"",L"Speakers (FengYin)"));
    assert(!isVirtual(L"",L"HD Audio Speaker 1"));
    assert(!isVirtual(L"",L"Realtek HD Audio 2nd output"));
}
'''
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder) / 'route.cpp'
            path.write_text(program, encoding='utf-8')
            binary = Path(folder) / 'route-test'
            subprocess.run([compiler, '-std=c++17', str(path), '-o', str(binary)], check=True, capture_output=True)
            subprocess.run([str(binary)], check=True, capture_output=True)

    def test_all_restore_targets_are_revalidated(self):
        source = (ROOT / 'audio-engine/windows/DefaultEndpointRouter.cpp').read_text(encoding='utf-8')
        active = source.split('bool isActiveEndpoint(', 1)[1].split('bool readPhysicalEndpoints', 1)[0]
        self.assertIn('!isVirtual(actualId, name)', active)
        restore = source.split('void DefaultEndpointRouter::restore()', 1)[1].split('bool DefaultEndpointRouter::restorePendingRoute', 1)[0]
        self.assertIn('restorePendingRoute(restoreError)', restore)
