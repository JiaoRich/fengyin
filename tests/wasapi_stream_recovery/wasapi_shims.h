// Minimal deterministic interface substitutes. NOT a Windows/driver test.
#include "AudioStreamRecovery.h"
#include <algorithm>
#include <atomic>
#include <cassert>
#include <chrono>
#include <ctime>
#include <functional>
#include <iostream>
#include <memory>
#include <thread>
#include <vector>
using UINT32=unsigned int; using DWORD=unsigned long; using uint8=unsigned char;
using IMMDevice=int;using HRESULT=int;
static double clockMs=0;
struct Time { static double getMillisecondCounterHiRes(){return clockMs;} };
constexpr int E_UNEXPECTED=-2,E_FAIL=-1,ERROR_TIMEOUT=1460,COINIT_MULTITHREADED=0;
int CoInitializeEx(void*,int){return 0;}void CoUninitialize(){}
bool FAILED(int h){return h<0;}bool SUCCEEDED(int h){return h>=0;}
int HRESULT_FROM_WIN32(int code){return -code;}
constexpr int S_OK=0, WAIT_OBJECT_0=0, WAIT_TIMEOUT=258;
constexpr int AUDCLNT_E_DEVICE_INVALIDATED=-2004287484;
static unsigned long waitCalls=0;
int WaitForSingleObject(int,unsigned) { ++waitCalls; clockMs+=10; return WAIT_OBJECT_0; }
bool check(int value) { return value>=0; }
template<class T> T jmax(T a,T b){return std::max(a,b);}
template<class T> T jmin(T a,T b){return std::min(a,b);}
#define jassert(x) assert(x)
enum class WASAPIDeviceMode {sharedLowLatency,exclusive};
bool isExclusiveMode(WASAPIDeviceMode m){return m==WASAPIDeviceMode::exclusive;}
struct Thread {
 virtual ~Thread()=default; virtual void run(){}
 unsigned long polls=0;bool running=true; double deadline=5000;
 std::function<void(unsigned long)> pollAction;
 bool threadShouldExit(){++polls;if(pollAction)pollAction(polls);return polls>20000||clockMs>=deadline;}
 bool isThreadRunning(){return running;}
 void wait(int ms){++waitCalls;clockMs+=ms;}
 static void sleep(int ms){clockMs+=ms;}
};
struct IOBase{virtual ~IOBase()=default;virtual bool isOpen(){return false;}virtual bool isPlaying(){return false;}};
struct Bits{int n=0;int countNumberOfSetBits()const{return n;}};
struct ScopedTryLock{explicit ScopedTryLock(int&){}bool isLocked()const{return true;}};
template<class T> struct AudioBuffer{
    std::vector<std::vector<T>> data;std::vector<T*> ptrs;std::vector<const T*> cptrs;
    AudioBuffer(int c,int n):data(c,std::vector<T>(n)){for(auto& v:data){ptrs.push_back(v.data());cptrs.push_back(v.data());}}
    void clear(){for(auto& v:data)std::fill(v.begin(),v.end(),0);}
    T*const* getArrayOfWritePointers(){return ptrs.data();}
    const T*const* getArrayOfReadPointers()const{return cptrs.data();}
};
struct Callback{unsigned calls=0;void audioDeviceIOCallbackWithContext(const float*const*,int,float*const*,int,int,int){++calls;}};
struct RunDevice{
    IMMDevice identity=1;IMMDevice*device=&identity;int clientEvent=1;unsigned copies=0;
    void handleDeviceBuffer(){}int getNumSamplesInReservoir(){return 128;}
    void copyBuffersFromReservoir(float*const*,int,int){}
    std::function<bool()>recover=[] {return false;};
 bool fengyinRecoverOutput(){return recover();}
 void fengyinFinishWorkerRecovery(){}
 void copyBuffers(const float*const*,int,int,RunDevice*,Thread&){++copies;clockMs+=1;}
};
struct FakeClient{int error=0;UINT32 padding=0;int GetCurrentPadding(UINT32*p){*p=padding;return error;}};
struct FakeRenderer{
    int getError=0,releaseError=0;unsigned gets=0,releases=0;uint8 data[4096]{};
    int GetBuffer(UINT32,uint8**p){++gets;*p=data;return getError;}
    int ReleaseBuffer(UINT32,unsigned){++releases;return releaseError;}
};
struct FakeConverter{unsigned calls=0;void convertSamples(uint8*,int,const float*,int,int){++calls;}};
struct Maps{int size(){return 2;}int getUnchecked(int x){return x;}};
struct WASAPIInputDevice{int clientEvent=1;void handleDeviceBuffer(){}};

// Explicit ownership/format substitutes for the extracted COM recovery methods.
// Thread ids are deterministic (0=UI, 1=worker); no COM driver is simulated.
#include <optional>
#include <utility>
static int ownerThread=0,wrongRelease=0,ownedLive=0,uiReleases=0,workerReleases=0;
struct OwnedInterface {
 int creator=ownerThread;
 OwnedInterface(){++ownedLive;}
 virtual ~OwnedInterface(){--ownedLive;if(creator!=ownerThread)++wrongRelease;
   if(ownerThread==0)++uiReleases;else ++workerReleases;}
};
template<class T> struct OwnedPtr {
 T* value=nullptr;
 OwnedPtr()=default;OwnedPtr(T* p):value(p){}
 OwnedPtr(OwnedPtr&& p)noexcept:value(std::exchange(p.value,nullptr)){}
 OwnedPtr& operator=(OwnedPtr&& p)noexcept{delete value;value=std::exchange(p.value,nullptr);return *this;}
 OwnedPtr& operator=(std::nullptr_t){delete value;value=nullptr;return *this;}
 ~OwnedPtr(){delete value;}
 T* operator->()const{return value;}operator T*()const{return value;}
 T** resetAndGetPointerAddress(){*this=nullptr;return &value;}
};
struct SessionSubstitute:OwnedInterface{void UnregisterAudioSessionNotification(void*){}};
struct CallbackSubstitute{std::atomic<bool>enabled{true};};
struct RenderSubstitute:OwnedInterface,FakeRenderer{};
struct ClientSubstitute:OwnedInterface,FakeClient {
 UINT32 frames=128;int eventError=0,serviceError=0,startError=0;
 int GetBufferSize(UINT32*p){*p=frames;return S_OK;}
 int SetEventHandle(int){return eventError;}
 void Stop(){}int Start(){return startError;}
 int GetService(int,void**p){if(serviceError)return serviceError;*p=new RenderSubstitute;return S_OK;}
};
constexpr int AUDCLNT_BUFFERFLAGS_SILENT=2;
void ResetEvent(int){}
bool isLowLatencyMode(WASAPIDeviceMode m){return m==WASAPIDeviceMode::sharedLowLatency;}
#define __uuidof(x) 1
