from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

class TechniqueMidiDeduplication(unittest.TestCase):
    def test_production_sender(self):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C++ compiler unavailable')
        root = Path(__file__).resolve().parents[1]
        source = r'''
#include "TechniqueMidiDeduplication.h"
#include <cassert>
#include <array>
int main() {
    std::atomic<int> cache {0}; int sent=0;
    auto ok=[&] {++sent;return true;};
    using fengyin::sendTechniqueController;
    assert(sendTechniqueController(cache,258,0,true,ok));
    assert(!sendTechniqueController(cache,258,0,true,ok));
    assert(sendTechniqueController(cache,258,1,true,ok));
    assert(sendTechniqueController(cache,258,0,true,ok));
    cache.store(0);
    assert(sendTechniqueController(cache,258,0,true,ok));
    assert(!sendTechniqueController(cache,258,80,true,[]{return false;}));
    assert(sendTechniqueController(cache,258,80,true,ok));
    assert(sendTechniqueController(cache,259,80,true,ok));
    assert(sendTechniqueController(cache,259,80,false,ok));
    assert(sendTechniqueController(cache,259,80,false,ok));
    assert(sent==8);
    std::array<std::atomic<int>,2> routes {}; routes[0].store(258); routes[1].store(259);
    std::array<std::atomic<int>,2> caches {};
    assert(sendTechniqueController(caches[0],258,0,true,ok));
    assert(sendTechniqueController(caches[1],259,0,true,ok));
    fengyin::invalidateTechniqueController(routes,caches,258);
    assert(sendTechniqueController(caches[0],258,0,true,ok));
    assert(!sendTechniqueController(caches[1],259,0,true,ok));
}
'''
        with tempfile.TemporaryDirectory() as folder:
            src = Path(folder)/'test.cpp'
            src.write_text(source)
            exe = Path(folder)/'test'
            subprocess.run([compiler,'-std=c++17','-I'+str(root/'native/Source'),str(src),'-o',str(exe)],check=True)
            subprocess.run([str(exe)],check=True)
