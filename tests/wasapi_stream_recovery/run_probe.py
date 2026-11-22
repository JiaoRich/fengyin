from pathlib import Path
import re,subprocess,hashlib,json,argparse,shutil
parser=argparse.ArgumentParser()
parser.add_argument("--juce-source",type=Path,required=True)
parser.add_argument("--output",type=Path,required=True)
args=parser.parse_args()
root=Path(__file__).resolve().parents[2]
out=args.output.resolve();out.mkdir(parents=True,exist_ok=True)
shutil.copyfile(Path(__file__).with_name("wasapi_shims.h"),out/"wasapi_shims.h")
s=(args.juce_source/'modules/juce_audio_devices/native/juce_WASAPI_windows.cpp').read_text()
provenance=[]
def extract(start,scope=0):
 p=s.index(start,scope);o=s.index('{',p);depth=0
 for m in re.finditer(r'//[^\n]*|/\*[\s\S]*?\*/|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|[{}]',s[o:]):
  if m.group()=='{':depth+=1
  elif m.group()=='}':
   depth-=1
   if depth==0:
    result=s[p:o+m.end()];provenance.append({'method':start,'line':s[:p].count('\n')+1,'sha256':hashlib.sha256(result.encode()).hexdigest()});return result
 raise RuntimeError(start)
d=s.index('class WASAPIAudioIODevice final');o=s.index('class WASAPIOutputDevice')
methods='\n'.join(extract(x,d).replace(' override','') for x in ('void run() override','bool isOpen() override','bool isPlaying() override','void deviceSessionBecameInactive','void deviceSessionBecameActive','void deviceSessionExpired','void fengyinStreamFailed','void fengyinSessionDisconnected'))
render='\n'.join(extract(x,o) for x in ('int getNumSamplesAvailableToCopy() const','void copyBuffers (const float* const*'))
base=s.index('class WASAPIDeviceBase')
lifecycle='''
struct LifecycleProbe {
 WASAPIDeviceMode deviceMode=WASAPIDeviceMode::sharedLowLatency;
 int numChannels=2,clientEvent=1;UINT32 actualBufferSize=128,nextFrames=128;
 IMMDevice identity=1;IMMDevice*device=&identity;
 OwnedPtr<ClientSubstitute> client{new ClientSubstitute},fengyinOriginalClient;
 OwnedPtr<SessionSubstitute> audioSessionControl{new SessionSubstitute},fengyinOriginalSession;
 OwnedPtr<CallbackSubstitute> sessionEventCallback{new CallbackSubstitute};
 OwnedPtr<RenderSubstitute> renderClient{new RenderSubstitute},fengyinOriginalRender;
 bool fengyinWorkerClient=false,fengyinWorkerRender=false,initOK=true,createOK=true;
 int eventError=0,serviceError=0,startError=0,initialisations=0,periodSeen=0,formatSeen=0,active=0;
 std::optional<int>fengyinOpenedFormat{48000};int fengyinOpenedPeriod=128;
 struct D{int hr=0,op=0;void fengyinStreamFailed(int h,int o){hr=h;op=o;}
 int active=0;void deviceSessionBecameActive(IMMDevice*){++active;}} delegateStorage;D&delegate=delegateStorage;
 OwnedPtr<ClientSubstitute> createClient(){if(!createOK)return {};auto*p=new ClientSubstitute;
 p->frames=nextFrames;p->eventError=eventError;p->serviceError=serviceError;p->startError=startError;return {p};}
 bool initialiseLowLatencyClient(int period,int format){++initialisations;periodSeen=period;formatSeen=format;return initOK;}
 bool initialiseStandardClient(int p,int f){return initialiseLowLatencyClient(p,f);}
 void deleteSessionEventCallback(){audioSessionControl=nullptr;sessionEventCallback=nullptr;}
 void createSessionEventCallback(){deleteSessionEventCallback();audioSessionControl=OwnedPtr<SessionSubstitute>{new SessionSubstitute};sessionEventCallback=OwnedPtr<CallbackSubstitute>{new CallbackSubstitute};}
'''+ '\n'.join(extract(x,base) for x in ('bool fengyinReopenSameClient()', 'void fengyinReleaseOriginalClient()', 'void closeClient()')) + '\n'.join(extract(x,o) for x in ('bool fengyinRecoverOutput()', 'bool start()', 'int getNumSamplesAvailableToCopy() const', 'void fengyinFinishWorkerRecovery()', 'void close()')) + '''
};
void checkLifecycle(){
 ownerThread=0;uiReleases=workerReleases=wrongRelease=0;
 {LifecycleProbe p;assert(ownedLive==3);ownerThread=1;
 assert(p.fengyinRecoverOutput());assert(uiReleases==0&&ownedLive==6);
 assert(p.periodSeen==128&&p.formatSeen==48000&&p.actualBufferSize==128);
 assert(p.fengyinRecoverOutput());assert(uiReleases==0&&ownedLive==6&&workerReleases==3);
 p.fengyinFinishWorkerRecovery();assert(ownedLive==3&&uiReleases==0);
 ownerThread=0;p.close();assert(ownedLive==0&&uiReleases==3&&wrongRelease==0);}
 for(int fail=0;fail<6;++fail){ownerThread=0;LifecycleProbe p;ownerThread=1;
 if(fail==0)p.createOK=false;if(fail==1)p.initOK=false;if(fail==2)p.nextFrames=480;
 if(fail==3)p.eventError=E_FAIL;if(fail==4)p.serviceError=E_FAIL;if(fail==5)p.startError=E_FAIL;
 assert(!p.fengyinRecoverOutput());assert(p.actualBufferSize==128);
 p.createOK=p.initOK=true;p.nextFrames=128;p.eventError=p.serviceError=p.startError=0;
 assert(p.fengyinRecoverOutput());p.fengyinFinishWorkerRecovery();ownerThread=0;p.close();
 assert(ownedLive==0&&wrongRelease==0);}
}
'''
cpp='''#include "wasapi_shims.h"
'''+extract('enum AudioSessionDisconnectReason')+''';
struct Backend:Thread,IOBase {
'''+extract('enum Flags',d)+''';
 std::atomic<int>flags{flagOpen|flagStarted|flagOutputIsActive};
 std::atomic<HRESULT>fengyinFault{0},fengyinLastError{0};
 std::atomic<int>fengyinLastOperation{0},fengyinDisconnect{-1},fengyinHealth{0},fengyinAttempts{0},fengyinRecoveries{0};
 int currentBufferSizeSamples=128,startStopLock=0;WASAPIDeviceMode deviceMode=WASAPIDeviceMode::sharedLowLatency;
 std::unique_ptr<RunDevice>inputDevice,outputDevice=std::make_unique<RunDevice>();
 Callback sink;Callback*callback=&sink;int updates=0;
 Bits getActiveInputChannels(){return {0};}Bits getActiveOutputChannels(){return {2};}
 void setMMThreadPriority(){}void triggerAsyncUpdate(){++updates;}
'''+methods+'''
};
struct EventOwner {struct D {int reason=-1;void fengyinSessionDisconnected(AudioSessionDisconnectReason r){reason=r;}} delegate;};
struct EventCallback {EventOwner owner;std::atomic<bool>enabled{true};
'''+extract('JUCE_COMRESULT OnSessionDisconnected').replace('JUCE_COMRESULT','int').replace(' override','')+'''
};
struct RendererProbe {
 int numChannels=2,actualNumChannels=2,clientEvent=1;UINT32 actualBufferSize=128;
 WASAPIDeviceMode deviceMode=WASAPIDeviceMode::sharedLowLatency;
 std::unique_ptr<FakeClient>client=std::make_unique<FakeClient>();std::unique_ptr<FakeRenderer>renderClient=std::make_unique<FakeRenderer>();
 std::unique_ptr<FakeConverter>converter=std::make_unique<FakeConverter>();Maps channelMaps;
 struct D {int hr=0,op=0;void fengyinStreamFailed(int h,int o){hr=h;op=o;}} storage; D& delegate=storage;
'''+render+'''
};
'''+lifecycle+'''
void run(Backend& b,double duration=5000){clockMs=0;waitCalls=0;b.deadline=duration;b.run();assert(b.polls<20000);}
int main(){
 checkLifecycle();
 Backend healthy;run(healthy);assert(healthy.sink.calls>4000&&healthy.fengyinAttempts==0&&waitCalls==0);
 Backend transient;transient.deviceSessionBecameInactive(transient.outputDevice->device);
 transient.pollAction=[&](auto){if(clockMs>=500)transient.deviceSessionBecameActive(transient.outputDevice->device);};
 run(transient);assert(transient.fengyinAttempts==0&&transient.sink.calls>4000&&waitCalls>0);
 Backend recovery;recovery.deviceSessionBecameInactive(recovery.outputDevice->device);
 recovery.outputDevice->recover=[&]{recovery.deviceSessionBecameActive(recovery.outputDevice->device);return true;};
 run(recovery);assert(recovery.fengyinRecoveries==1&&recovery.sink.calls>3000&&recovery.fengyinHealth==0);
 Backend exhausted;exhausted.fengyinStreamFailed(AUDCLNT_E_DEVICE_INVALIDATED,2);run(exhausted,12000);
 assert(exhausted.fengyinAttempts==3&&exhausted.fengyinHealth==2&&exhausted.sink.calls==0);
 Backend stop;stop.deviceSessionBecameInactive(stop.outputDevice->device);stop.pollAction=[&](auto){if(clockMs>300)stop.flags.fetch_and(~Backend::flagStarted);};
 run(stop);assert(stop.fengyinAttempts==0&&stop.sink.calls==0);
 Backend logoff;logoff.fengyinSessionDisconnected(DisconnectReasonSessionLogoff);run(logoff);
 assert(logoff.fengyinAttempts==0&&logoff.sink.calls==0&&logoff.fengyinHealth==2);
 Backend expired;expired.deviceSessionExpired(expired.outputDevice->device);
 expired.outputDevice->recover=[&]{expired.deviceSessionBecameActive(expired.outputDevice->device);return true;};run(expired);
 assert(expired.fengyinRecoveries==1&&expired.updates==0);
 for(int r=0;r<6;++r){EventCallback e;e.OnSessionDisconnected(static_cast<AudioSessionDisconnectReason>(r));assert(e.owner.delegate.reason==r);
 e.enabled=false;e.OnSessionDisconnected(static_cast<AudioSessionDisconnectReason>((r+1)%6));assert(e.owner.delegate.reason==r);}
 float data[128]{};const float* src[]{data,data};Thread t;t.deadline=1e9;
 for(int failure:{1,2,3}){RendererProbe p;if(failure==1)p.client->error=AUDCLNT_E_DEVICE_INVALIDATED;
 if(failure==2)p.renderClient->getError=AUDCLNT_E_DEVICE_INVALIDATED;
 if(failure==3)p.renderClient->releaseError=AUDCLNT_E_DEVICE_INVALIDATED;
 p.copyBuffers(src,2,128,nullptr,t);assert(p.delegate.hr==AUDCLNT_E_DEVICE_INVALIDATED&&p.delegate.op==failure);}
 RendererProbe full;full.client->padding=128;clockMs=0;full.copyBuffers(src,2,128,nullptr,t);
 assert(full.delegate.op==4&&clockMs>=1000&&clockMs<1100);
 RendererProbe ok;ok.copyBuffers(src,2,128,nullptr,t);assert(ok.delegate.op==0&&ok.renderClient->releases==1);
 std::cout<<"Extracted production WASAPI: COM ownership/exact format/rebuild failure cleanup, healthy output, transient Inactive, same-client recovery, retry limit, manual stop, logoff, expired, all disconnect reasons, HRESULT errors and non-progress timeout passed. NOT a Windows driver test.\\n";
}
'''
(out/'wasapi_probe.cpp').write_text(cpp)
(out/'wasapi-provenance.json').write_text(json.dumps(provenance,indent=2))
r=subprocess.run(['clang++','-std=c++20','-O2','-I',str(root/'native/Source'),str(out/'wasapi_probe.cpp'),'-o',str(out/'wasapi_probe')],capture_output=True,text=True)
(out/'wasapi-build.log').write_text(r.stdout+r.stderr);r.check_returncode()
r=subprocess.run([str(out/'wasapi_probe')],capture_output=True,text=True,timeout=10);(out/'wasapi-tests.txt').write_text(r.stdout+r.stderr);print(r.stdout+r.stderr);r.check_returncode()
