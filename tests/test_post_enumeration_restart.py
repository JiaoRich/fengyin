from pathlib import Path
import unittest
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]


class PostEnumerationRestart(unittest.TestCase):
    def test_windows_pixel_reader_return_types(self):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C++ compiler unavailable')
        code=(ROOT/'tests/test_compact_glow.cpp').read_text(encoding='utf-8')
        start=code.index('        const auto read =')
        end=code.index('\n        };',start)+len('\n        };')
        # Windows COLORREF is unsigned long, while CLR_INVALID is an unsigned
        # int literal. Equal widths do not make lambda deduction types equal.
        source='''#include <cassert>
using COLORREF=unsigned long;
constexpr unsigned int CLR_INVALID=0xffffffffu;
bool available=true;
void* GetDC(void*) { return available ? &available : nullptr; }
COLORREF GetPixel(void*,int,int) { return 0x214365ul; }
void ReleaseDC(void*,void*) {}
int main() {
''' + code[start:end] + '''
assert(read(0,0)==0x214365ul);
available=false;
assert(read(0,0)==CLR_INVALID);
}
'''
        with tempfile.TemporaryDirectory() as directory:
            cpp=Path(directory)/'pixel.cpp';binary=Path(directory)/'pixel'
            cpp.write_text(source)
            subprocess.run([compiler,'-std=c++17',str(cpp),'-o',str(binary)],check=True,capture_output=True)
            subprocess.run([str(binary)],check=True)

    def test_production_restart_precedes_open_even_with_zero_changes(self):
        code=(ROOT/'audio-engine/windows/Asio4AllOutput.cpp').read_text(encoding='utf-8')
        start=code.index('if (configuration.enumerated)')
        reset=code.index('requestFengYinAsioReinitialisation()',start)
        opening=code.index('device->open(',reset)
        self.assertLess(start,reset)
        self.assertLess(reset,opening)
        self.assertNotIn('configuration.configured',code[start:reset])
        api=(ROOT/'audio-engine/windows/RealtekAsioConfiguration.h').read_text(encoding='utf-8')
        self.assertLess(api.index('api->enumerate();'),api.index('context.result.enumerated = true'))

    def test_halo_uses_software_composition_and_dwm_not_only_binary_region(self):
        halo=(ROOT/'native/Source/CompactWindowGlow.h').read_text(encoding='utf-8')
        window=(ROOT/'native/Source/Main.cpp').read_text(encoding='utf-8')
        self.assertIn('setCurrentRenderingEngine(0)',halo)
        self.assertIn('HWND_TOP',halo)
        self.assertLess(window.index('DwmSetWindowAttribute'),window.index('CreateRoundRectRgn'))
