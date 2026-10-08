"""Run the complete production ASIO enumeration callback against a fake ABI."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]

class RealtekAsioExecution(unittest.TestCase):
    def test_enumeration_transaction_and_callback_scope(self):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C++ compiler unavailable')
        source = (ROOT/'audio-engine/windows/RealtekAsioConfiguration.h').read_text(encoding='utf-8')
        source = '\n'.join(line for line in source.splitlines() if not line.startswith('#'))
        shim = r'''
#include "audio-engine/windows/RealtekOutputPolicy.h"
#include <string>
#include <algorithm>
#include <map>
#include <tuple>
#include <cassert>
#include <cstring>
using DWORD=std::uint32_t;using BOOL=int;
constexpr int FALSE=0,KSPIN_DATAFLOW_IN=1;
struct GUID {unsigned a;unsigned short b,c;unsigned char d[8];};
#define FAILED(hr) ((hr)<0)
namespace juce {
struct String:std::string {
 using std::string::string;
 String(const std::string& s):std::string(s){}
 String(int i):std::string(std::to_string(i)){}
 String(long i):std::string(std::to_string(i)){}
 String(const wchar_t* p){while(*p)push_back(char(*p++));}
 static String fromUTF8(const char* p){return p;}
 static String toHexString(int i){return String(i);}
 String lower()const{String s=*this;std::transform(s.begin(),s.end(),s.begin(),::tolower);return s;}
 bool startsWithIgnoreCase(String s)const{return lower().rfind(s.lower(),0)==0;}
 bool containsIgnoreCase(String s)const{return lower().find(s.lower())!=npos;}
};struct Logger {static void writeToLog(const String&) {}};
}
namespace Microsoft::WRL {template<class T> struct ComPtr {
 T* p=nullptr;T** GetAddressOf(){return &p;}T* Get(){return p;}T* operator->(){return p;}
};}
struct IUnknown {virtual int QueryInterface(GUID,void**)=0;};
struct A4Private: IUnknown {
 using Key=std::tuple<long,long,long>;
 std::map<Key,DWORD> flags;
 std::wstring names[2]={L"VB-Audio Virtual Cable",L"Realtek(R) Audio"};
 std::wstring ids[2]={L"ROOT\\MEDIA\\0000",L"HDAUDIO\\FUNC_01&VEN_10EC&DEV_0256"};
 BOOL(*cb)(void*)=nullptr;void* context=nullptr;bool inside=false;
 int enumeration=0,writes=0,failWrite=-1;bool failRead=false,resetOnRefresh=false;
 int QueryInterface(GUID,void** out)override{*out=this;return 0;}
 void callback(BOOL(*f)(void*),void* c){cb=f;context=c;}
 void enumerate(){for(int n=0;n<3;++n){++enumeration;if(n==1&&resetOnRefresh)flags[{1,0,1}]&=~0x80000000u;inside=true;bool again=cb(context);inside=false;if(!again)return;}assert(false);}
 DWORD getDevice(int prop,long d,void* data,long){assert(inside);
  if(!flags.count({d,-1,-1}))return 0xA4AE1001u;
  if(prop==0){*static_cast<DWORD*>(data)=flags[{d,-1,-1}];return 0;}
  *static_cast<const wchar_t**>(data)=prop==1?names[d].c_str():ids[d].c_str();return 0;
 }
 DWORD getInterface(int,long d,long i,void* data,long){assert(inside);
  if(!flags.count({d,i,-1}))return 0xA4AE1001u;
  *static_cast<DWORD*>(data)=flags[{d,i,-1}];return 0;
 }
 DWORD getPin(int prop,long d,long i,long p,void* data,long){assert(inside);
  if(!flags.count({d,i,p}))return 0xA4AE1001u;
  if(failRead&&p==1)return 0xA4AE2001u;
  *static_cast<DWORD*>(data)=prop==0?flags[{d,i,p}]:prop==1?(p==2?2:1):2;return 0;
 }
 DWORD write(Key k,void* data){assert(inside);flags[k]=*static_cast<DWORD*>(data);return writes++==failWrite?0xA4AE2001u:0;}
 DWORD setDevice(int,long d,void* p,long){return write({d,-1,-1},p);}
 DWORD setInterface(int,long d,long i,void* p,long){return write({d,i,-1},p);}
 DWORD setPin(int,long d,long i,long n,void* p,long){return write({d,i,n},p);}
};
'''
        checks = r'''
int main(){using namespace fengyin::audioengine;
 A4Private initial;
 for(int d=0;d<2;++d){initial.flags[{d,-1,-1}]=d?0:0x80000000u;initial.flags[{d,0,-1}]=d?0:0x80000000u;
  for(int p=0;p<3;++p)initial.flags[{d,0,p}]=0x40000000u|(d?0:0x80000000u);}
 auto readOnly=initial;auto observed=configureRealtekOutputs(&readOnly,true);
 assert(readOnly.writes==0 && readOnly.flags==initial.flags && readOnly.enumeration==1 && readOnly.cb==nullptr);
 assert(!observed.configured && observed.rollbackOK && observed.enumerated);
 for(int fail=-1;fail<10;++fail){auto api=initial;api.failWrite=fail;auto r=configureRealtekOutputs(&api);
  assert(api.cb==nullptr);assert(api.enumeration<=2);
  if(!r.configured){assert(r.rollbackOK);assert(api.flags==initial.flags);}
  else {assert(!(api.flags[{0,0,0}]&0x80000000u));assert((api.flags[{1,0,0}]&0x80000000u));
   assert((api.flags[{1,0,1}]&0x80000000u));assert(!(api.flags[{1,0,2}]&0x80000000u));
   api.failWrite=-1;api.writes=0;assert(configureRealtekOutputs(&api).configured);assert(api.writes==0);}
 }
 auto partial=initial;partial.failRead=true;assert(!configureRealtekOutputs(&partial).configured);assert(partial.writes==0);
 auto unknown=initial;unknown.ids[1]=L"ROOT\\UNKNOWN";assert(!configureRealtekOutputs(&unknown).configured);assert(unknown.writes==0);
 auto reset=initial;reset.resetOnRefresh=true;auto result=configureRealtekOutputs(&reset);assert(!result.configured&&!result.rollbackOK);
}
'''
        with tempfile.TemporaryDirectory() as temp:
            cpp = Path(temp)/'test.cpp'
            exe = Path(temp)/'test'
            cpp.write_text(shim+source+checks, encoding='utf-8')
            subprocess.run([compiler,'-std=c++17','-I',str(ROOT),str(cpp),'-o',str(exe)],check=True)
            subprocess.run([str(exe)],check=True)

if __name__ == '__main__':
    unittest.main()
