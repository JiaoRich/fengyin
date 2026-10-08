"""Execute the production topology walk with a split jack/wave mock graph."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class TopologyExecution(unittest.TestCase):
    def test_actual_walk_resolves_stream_not_jack_and_rejects_ambiguity(self):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C++ compiler unavailable')
        source = (ROOT / 'audio-engine/windows/AsioEndpointSelection.h').read_text(encoding='utf-8')
        walk = source.split('    struct StreamPin {', 1)[1].split('    // An ambiguous/offload', 1)[0]
        program = r'''
#include <vector>
#include <set>
#include <string>
#include <functional>
#include <cassert>
using UINT=unsigned; using LPWSTR=wchar_t*; using HRESULT=int;
constexpr int E_NOTFOUND=-1;
#define FAILED(x) ((x)<0)
#define SUCCEEDED(x) ((x)>=0)
#define IID_PPV_ARGS(x) x
void CoTaskMemFree(void*) {}
enum ConnectorType {Physical_External, Software_Fixed, Software_IO};
enum DataFlow {In,Out};
namespace juce { using String=std::wstring; }
template<class T> struct ComPtr {
 T* p=nullptr; T* Get() const {return p;} T* operator->(){return p;}
 T** operator&(){return &p;}
 template<class U> int As(U** out){*out=p;return p?0:-2;}
};
struct IPart;
using IConnector=IPart;
struct IDeviceTopology {
 std::wstring path;
 int GetDeviceId(LPWSTR* p){*p=path.data();return 0;}
};
struct IPartsList {
 std::vector<IPart*> nodes;
 int GetCount(UINT* n){*n=nodes.size();return 0;}
 int GetPart(UINT n,IPart** p){*p=nodes.at(n);return 0;}
};
struct IPart {
 std::wstring id; UINT local=0; bool connector=true;
 ConnectorType kind=Physical_External; DataFlow flow=Out;
 IPart* connected=nullptr; IPartsList incoming; IDeviceTopology owner;
 int GetGlobalId(LPWSTR* p){*p=id.data();return 0;}
 int QueryInterface(IPart** p){*p=connector?this:nullptr;return connector?0:-2;}
 int GetType(ConnectorType* p){*p=kind;return 0;}
 int GetDataFlow(DataFlow* p){*p=flow;return 0;}
 int GetTopologyObject(IDeviceTopology** p){*p=&owner;return 0;}
 int GetLocalId(UINT* p){*p=local;return 0;}
 int GetConnectedTo(IPart** p){*p=connected;return connected?0:-2;}
 int EnumPartsIncoming(IPartsList** p){*p=&incoming;return incoming.nodes.empty()?E_NOTFOUND:0;}
};
bool resolve(IPart* start,std::wstring& path,long& pin) {
 ComPtr<IPart> part; part.p=start;
''' + '    struct StreamPin {' + walk + r'''
 if(!complete || streams.size()!=1) return false;
 path=streams[0].path;pin=streams[0].pin;return true;
}
int main() {
 IPart jack, bridge, waveBridge, stream, second;
 jack.id=L"jack";jack.local=7;jack.owner.path=L"topology";
 bridge.id=L"bridge";bridge.kind=Software_Fixed;bridge.flow=In;
 waveBridge.id=L"wave-bridge";waveBridge.kind=Software_Fixed;
 stream.id=L"stream";stream.kind=Software_IO;stream.flow=In;
 stream.local=0;stream.owner.path=L"wave";
 jack.incoming.nodes={&bridge};bridge.connected=&waveBridge;
 waveBridge.incoming.nodes={&stream};
 std::wstring path;long pin=-1;
 assert(resolve(&jack,path,pin));assert(path==L"wave" && pin==0);
 second=stream;second.id=L"offload";second.local=1;
 waveBridge.incoming.nodes.push_back(&second);
 assert(!resolve(&jack,path,pin));
 waveBridge.incoming.nodes={&waveBridge}; // Cycle terminates, no guessed pin.
 assert(!resolve(&jack,path,pin));
 waveBridge.incoming.nodes={&stream};bridge.connected=nullptr;
 assert(!resolve(&jack,path,pin));
 bridge.connected=&waveBridge;stream.flow=Out; // Capture is not render.
 assert(!resolve(&jack,path,pin));
}
'''
        with tempfile.TemporaryDirectory() as folder:
            cpp = Path(folder) / 'topology.cpp'
            binary = Path(folder) / 'topology-test'
            cpp.write_text(program, encoding='utf-8')
            subprocess.run([compiler, '-std=c++17', str(cpp), '-o', str(binary)],
                           check=True, capture_output=True)
            subprocess.run([str(binary)], check=True, capture_output=True)
