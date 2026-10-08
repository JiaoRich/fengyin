"""Execute the exact reset-handling block inserted into JUCE, using a fake driver.

This tests lifecycle ordering, not Windows hardware compatibility.
"""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class AsioResetExecution(unittest.TestCase):
    def test_latched_reset_and_failure_ordering(self):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C++ compiler unavailable')
        patch = (ROOT / 'scripts/patch-juce-wasapi-raw.cmake').read_text(encoding='utf-8')
        begin = patch.index('        if (fengyinResetPending.exchange (false))')
        end = patch.index('\n       #endif', begin)
        block = patch[begin:end]
        source = r'''
#include <atomic>
#include <cassert>
#include <string>
#include <vector>
#define JUCE_ASIO_LOG(x) ((void)0)
struct String : std::string {
    using std::string::string;
    bool isNotEmpty() const { return !empty(); }
};
struct Driver {
    std::atomic<bool> fengyinResetPending{false};
    bool timer = false, needToReset = false;
    bool closeOK = true, loadOK = true, initOK = true;
    std::vector<std::string> events;
    void resetRequest() { fengyinResetPending.store(true); timer = true; }
    void stopTimer() { timer = false; }
    bool removeCurrentDriver() { events.push_back("close"); return closeOK; }
    bool loadDriver() { events.push_back("load"); return loadOK; }
    String initDriver() { events.push_back("init"); return initOK ? "" : "init failed"; }
    void reloadChannelNames() { events.push_back("names"); }
    String open() {
''' + block + r'''
        events.push_back("channels");
        events.push_back("buffers");
        events.push_back("start");
        return "";
    }
};
int main() {
    Driver normal;
    assert(normal.open().empty());
    assert((normal.events == std::vector<std::string>{"channels","buffers","start"}));
    Driver pending;
    pending.resetRequest(); pending.stopTimer(); // constructor/close cancelled delivery
    assert(pending.open().empty());
    assert((pending.events == std::vector<std::string>{"close","load","init","names","channels","buffers","start"}));
    pending.events.clear();
    assert(pending.open().empty()); // consumed once, not an endless reset loop
    assert((pending.events == normal.events));
    for (int failure=0; failure<3; ++failure) {
        Driver d; d.resetRequest(); d.stopTimer();
        d.closeOK = failure != 0; d.loadOK = failure != 1; d.initOK = failure != 2;
        assert(d.open().isNotEmpty());
        for (auto& event : d.events) assert(event != "buffers" && event != "start");
    }
}
'''
        with tempfile.TemporaryDirectory(prefix='fengyin-asio-reset-') as directory:
            cpp = Path(directory) / 'reset.cpp'
            binary = Path(directory) / 'reset-test'
            cpp.write_text(source, encoding='utf-8')
            subprocess.run([compiler, '-std=c++17', str(cpp), '-o', str(binary)], check=True,
                           capture_output=True, text=True)
            subprocess.run([str(binary)], check=True, capture_output=True, text=True)


if __name__ == '__main__':
    unittest.main()
