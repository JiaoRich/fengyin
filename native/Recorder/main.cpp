// FengYin UI/control protocol; recording itself belongs to upstream libraries.
// No CLR, microphone, instrument processing, auto desktop fallback or drivers.
#define NOMINMAX
#include <windows.h>
#include <shellapi.h>
#include <mmdeviceapi.h>
// SRL includes D3D10.1 transitively. The Windows SDK requires this superset
// before d3d10.h; it also declares the ID3D10Multithread self-test interface.
#include <d3d10_1.h>
#include <wrl/client.h>
#include <winrt/base.h>
#include <filesystem>
#include <fstream>
#include <atomic>
#include <mutex>
#include <thread>
#include <stdexcept>
#include <cmath>
#include <algorithm>
#include <cstring>
#include <cstdint>
#include "RecordingManager.h"
#include "MiniAudioRecorder.h"
#include "RecorderPolicy.h"
#include "RecordingStageTiming.h"
using Microsoft::WRL::ComPtr;
using namespace fengyin::capture;

namespace {
void checked(HRESULT hr){winrt::check_hresult(hr);}
bool status(const std::filesystem::path& path,const std::string& text){
    const auto temporary=path.wstring()+L".tmp";
    {std::ofstream out(temporary,std::ios::binary|std::ios::trunc);out<<text;out.flush();if(!out)return false;}
    return MoveFileExW(temporary.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=0;
}
struct Endpoint {
    ComPtr<IMMDeviceEnumerator> enumerator;
    std::wstring id;
    std::wstring current(){
        ComPtr<IMMDevice> device;checked(enumerator->GetDefaultAudioEndpoint(eRender,eMultimedia,&device));
        DWORD state=0;checked(device->GetState(&state));
        if(state!=DEVICE_STATE_ACTIVE)throw std::runtime_error("System output device is not active");
        LPWSTR value=nullptr;checked(device->GetId(&value));std::wstring result=value;CoTaskMemFree(value);return result;
    }
    Endpoint(){checked(CoCreateInstance(__uuidof(MMDeviceEnumerator),nullptr,CLSCTX_ALL,IID_PPV_ARGS(&enumerator)));id=current();}
    bool unchanged(){return id==current();}
};
struct Session {
    std::atomic<bool> ready{false},complete{false},failed{false};
    std::atomic<int> frames{0};std::mutex mutex;std::wstring error;
    bool measure=false;
    RecordingStageTiming timing;
};
Session* session=nullptr; // One recorder/session per isolated helper process.
void __stdcall completed(std::wstring,nlohmann::fifo_map<std::wstring,int>){session->complete.store(true);}
void __stdcall stateChanged(int value){if(value==STATUS_RECORDING)session->ready.store(true);}
void __stdcall failure(std::wstring message,std::wstring){
    {std::lock_guard<std::mutex> lock(session->mutex);session->error=std::move(message);}
    session->failed.store(true);
}
void __stdcall audioPacket(FRAME_AUDIO_INFO* info){
    if(session->measure)session->timing.audio(GetTickCount64(),info?info->PaddedBytes:0);
}
void __stdcall frame(int number,INT64,FRAME_BITMAP_DATA*){
    session->frames.store(number+1);
    if(session->measure)session->timing.frame(GetTickCount64());
}
std::unique_ptr<H264_ENCODER_OPTIONS> encoderOptions(){
    auto options=std::make_unique<H264_ENCODER_OPTIONS>();
    options->SetVideoFps(fps);options->SetVideoBitrate(bitrate);
    options->SetVideoBitrateMode(eAVEncCommonRateControlMode_CBR);
    options->SetFixedFramerate(true);options->SetHardwareEncodingEnabled(true);
#if defined(FENGYIN_RECORDER_EXPLICIT_RATE_PROBE)
    // Diagnostic target only: compare upstream's explicit input-rate branch.
    // Never defined for the application recorder or shipped as a replacement.
    options->SetFixedFramerate(false);
#endif
    options->SetThrottlingDisabled(false);options->SetFastStartEnabled(false);
    return options;
}
// Actual upstream encoder, GPU texture input and audio muxing; no speaker needed.
// This is NOT a desktop-capture or target-machine performance acceptance test.
void videoSelfTest(const std::filesystem::path& path){
    checked(MFStartup(MF_VERSION));
    {
        ComPtr<ID3D11Device> device;ComPtr<ID3D11DeviceContext> context;
        auto hr=D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,D3D11_CREATE_DEVICE_BGRA_SUPPORT,
            nullptr,0,D3D11_SDK_VERSION,&device,nullptr,&context);
        if(FAILED(hr))checked(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,D3D11_CREATE_DEVICE_BGRA_SUPPORT,
            nullptr,0,D3D11_SDK_VERSION,&device,nullptr,&context));
        ComPtr<ID3D10Multithread> multithread;
        checked(context.As(&multithread));multithread->SetMultithreadProtected(TRUE);
        TimelineManager timeline;checked(timeline.Initialize(1000.0/fps,10000));
        std::shared_ptr<ENCODER_OPTIONS> enc=encoderOptions();
        auto audio=std::make_shared<AUDIO_OPTIONS>();audio->SetAudioEnabled(true);audio->SetAudioChannels(2);audio->SetAudioBitrate(audioBitrateBytes);
        auto snapshot=std::make_shared<SNAPSHOT_OPTIONS>();auto output=std::make_shared<OUTPUT_OPTIONS>();
        OutputManager writer;checked(writer.Initialize(context.Get(),device.Get(),&timeline,enc,audio,snapshot,output));
        checked(writer.BeginRecording(path.wstring(),SIZE{1920,1080}));
        std::vector<uint32_t> pixels(1920*1080);
        for(int y=0;y<1080;++y)std::fill_n(pixels.data()+y*1920,1920,y<540?0xffff0000:0xff0000ff);
        D3D11_TEXTURE2D_DESC desc{};desc.Width=1920;desc.Height=1080;desc.MipLevels=1;desc.ArraySize=1;
        desc.Format=DXGI_FORMAT_B8G8R8A8_UNORM;desc.SampleDesc.Count=1;desc.Usage=D3D11_USAGE_DEFAULT;
        desc.BindFlags=D3D11_BIND_SHADER_RESOURCE|D3D11_BIND_RENDER_TARGET;
        D3D11_SUBRESOURCE_DATA initial{pixels.data(),1920*4,0};
        ComPtr<ID3D11Texture2D> texture;checked(device->CreateTexture2D(&desc,&initial,&texture));
        for(int n=0;n<48;++n){
            FrameWriteModel model{};model.Frame=texture.Get();model.VideoStartPos=n*10000000LL/fps;
            model.VideoDuration=(n+1)*10000000LL/fps-model.VideoStartPos;
            model.AudioStartPos=n*2000LL*10000000/48000;model.AudioDuration=2000LL*10000000/48000;
            model.Audio.resize(2000*4);
            for(int i=0;i<2000;++i){int16_t sample=static_cast<int16_t>(std::sin((n*2000+i)*6.283185307179586*440/48000)*3000);
                std::memcpy(model.Audio.data()+i*4,&sample,2);std::memcpy(model.Audio.data()+i*4+2,&sample,2);}
            checked(writer.RenderFrame(model));
        }
        checked(writer.FinalizeRecording());
    }
    MFShutdown();
}
}
int WINAPI wWinMain(HINSTANCE,HINSTANCE,PWSTR,int){
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    int argc=0;auto raw=CommandLineToArgvW(GetCommandLineW(),&argc);
    std::vector<std::wstring> args;if(raw){for(int n=0;n<argc;++n)args.emplace_back(raw[n]);LocalFree(raw);}
    std::filesystem::path output,state;
    HANDLE parent=nullptr;
    try{
        winrt::init_apartment(winrt::apartment_type::multi_threaded);
        if(argc==3 && (args[1]==L"--selftest" || args[1]==L"--audio-selftest")){
            output=args[2];state=output.wstring()+L".error.txt";
            if(args[1]==L"--selftest")videoSelfTest(output);else MiniAudioRecorder::selfTest(output);
            return 0;
        }
        if(argc!=8)throw std::runtime_error("Invalid recording arguments");
        output=args[2];state=args[4];const std::filesystem::path stop=args[3];
        if(std::filesystem::exists(output))throw std::runtime_error("Refusing to overwrite an existing recording");
        const bool audioOnly=args[1]==L"audio";
        if(!audioOnly && args[1].rfind(L"screen:",0)!=0)throw std::runtime_error("Expected explicit screen capture scope");
        parent=OpenProcess(SYNCHRONIZE,FALSE,static_cast<DWORD>(std::stoul(args[7])));
        if(!parent)throw std::runtime_error("FengYin parent process unavailable");
        const auto stopRequested=[&]{return std::filesystem::exists(stop) || WaitForSingleObject(parent,0)==WAIT_OBJECT_0;};
        Endpoint endpoint;auto nextEndpointCheck=GetTickCount64()+1000;
        if(audioOnly){
            MiniAudioRecorder audio;audio.start(output,endpoint.id);
            if(!status(state,"recording"))throw std::runtime_error("Cannot write recording status");
            while(!stopRequested()){
                audio.drain();
                if(GetTickCount64()>=nextEndpointCheck){if(!endpoint.unchanged())throw std::runtime_error("System output changed; recording stopped");nextEndpointCheck=GetTickCount64()+1000;}
                Sleep(10);
            }
            audio.finish();
        }else{
            const auto hwnd=reinterpret_cast<HWND>(static_cast<uintptr_t>(std::stoull(args[1].substr(7),nullptr,16)));
            if(!IsWindow(hwnd))throw std::runtime_error("FengYin window unavailable");
            // Pin the display at start. Moving/minimising the app cannot silently
            // change capture scope. Both full and compact mode use this rule.
            const auto monitor=MonitorFromWindow(hwnd,MONITOR_DEFAULTTONEAREST);
            MONITORINFOEXW info{};info.cbSize=sizeof(info);
            if(!GetMonitorInfoW(monitor,&info))throw std::runtime_error("Cannot resolve recording display");
            const auto size=canvas(info.rcMonitor.right-info.rcMonitor.left,info.rcMonitor.bottom-info.rcMonitor.top);
            Session progress;session=&progress;
            wchar_t diagnostic[2]{};
            progress.measure=GetEnvironmentVariableW(L"FENGYIN_RECORDING_DIAGNOSTICS",diagnostic,2)==1 && diagnostic[0]==L'1';
            std::ofstream timing;
            if(progress.measure)timing.open(output.wstring()+L".timing.log",std::ios::binary);
            auto nextTiming=GetTickCount64()+1000;
            RecordingManager recorder;
            recorder.RecordingCompleteCallback=completed;recorder.RecordingFailedCallback=failure;
            recorder.RecordingStatusChangedCallback=stateChanged;recorder.RecordingFrameNumberChangedCallback=frame;
            if(progress.measure)recorder.RecordingNewAudioCallback=audioPacket;
            recorder.SetLogEnabled(true);recorder.SetLogSeverityLevel(LOG_LVL_WARN);
            recorder.SetLogFilePath(output.wstring()+L".log");
            recorder.SetEncoderOptions(encoderOptions().release());
            auto target=new OUTPUT_OPTIONS;target->SetFrameSize(SIZE{size.width,size.height});
            target->SetStretch(TextureStretchMode::Uniform);target->SetVideoFramePreviewEnabled(false);recorder.SetOutputOptions(target);
            RECORDING_SOURCE screen;screen.ID=L"fengyin-selected-display";screen.Type=RecordingSourceType::Display;
            screen.SourcePath=info.szDevice;screen.SourceApi=RecordingSourceApi::WindowsGraphicsCapture;
            screen.IsCursorCaptureEnabled=true;screen.IsBorderRequired=true;screen.IsVideoFramePreviewEnabled=false;
            std::vector<RECORDING_SOURCE> sources{screen};recorder.SetRecordingSources(sources);
            auto audio=new AUDIO_OPTIONS;audio->SetAudioEnabled(true);audio->SetAudioChannels(2);audio->SetAudioBitrate(audioBitrateBytes);
            AUDIO_SOURCE speaker; speaker.ID=L"fengyin-system-output";speaker.Kind=AudioClientKind::EndpointLoopback;
            speaker.DeviceName=endpoint.id; // Explicit eMultimedia, not library eConsole fallback.
            std::vector<AUDIO_SOURCE> sounds{speaker};audio->SetAudioSources(sounds,false);recorder.SetAudioOptions(audio);
            auto mouse=new MOUSE_OPTIONS;mouse->SetDetectMouseClicks(false);mouse->SetMousePointerEnabled(true);recorder.SetMouseOptions(mouse);
            checked(recorder.BeginRecording(output.wstring()));
            const auto started=GetTickCount64();bool announced=false;std::string deferredError;
            while(!progress.complete.load()&&!progress.failed.load()){
                if(progress.ready.load()&&!announced){
                    if(!status(state,"recording")){deferredError="Cannot write recording status";break;}announced=true;
                }
                if(stopRequested())break;
                if(progress.measure && GetTickCount64()>=nextTiming){
                    const auto now=GetTickCount64();const auto sample=progress.timing.take(now);
                    timing<<"wallMs="<<now<<" frames="<<progress.frames.load()
                        <<" audioPackets="<<sample.audioPackets<<" paddedBytes="<<sample.paddedBytes
                        <<" postAudioToFrameMaxMs="<<sample.postAudioToFrameMaxMs
                        <<" frameIntervalMaxMs="<<sample.frameIntervalMaxMs
                        <<" audioGapMs="<<sample.audioGapMs<<" frameGapMs="<<sample.frameGapMs<<'\n';
                    timing.flush();nextTiming=now+1000;
                }
                if(!announced && GetTickCount64()-started>18000){deferredError="Screen recording startup timed out";break;}
                if(GetTickCount64()>=nextEndpointCheck){
                    try{if(!endpoint.unchanged())deferredError="System output changed; recording stopped";}
                    catch(...){deferredError="System output became unavailable; recording stopped";}
                    MONITORINFOEXW current{};current.cbSize=sizeof(current);
                    if(!GetMonitorInfoW(monitor,&current) || std::wstring(current.szDevice)!=info.szDevice
                        || !EqualRect(&current.rcMonitor,&info.rcMonitor))deferredError="Recording display changed; recording stopped";
                    if(!deferredError.empty())break;nextEndpointCheck=GetTickCount64()+1000;
                }
                Sleep(20);
            }
            recorder.EndRecording();
            const auto deadline=GetTickCount64()+12000;
            while(!progress.complete.load()&&!progress.failed.load()&&GetTickCount64()<deadline)Sleep(20);
            if(!progress.complete.load()&&!progress.failed.load()){
                status(state,"Recording finalization timed out; partial file retained");CloseHandle(parent);ExitProcess(4);
            }
            if(progress.failed.load()){std::lock_guard<std::mutex> lock(progress.mutex);throw std::runtime_error(winrt::to_string(progress.error));}
            if(!deferredError.empty())throw std::runtime_error(deferredError);
            if(!announced || progress.frames.load()==0)throw std::runtime_error("No captured video frames");
        }
        CloseHandle(parent);parent=nullptr;
        return status(state,"saved")?0:3;
    }catch(const winrt::hresult_error& e){if(!state.empty())status(state,winrt::to_string(e.message())+" ("+std::to_string(static_cast<long>(e.code()))+")");}
    catch(const std::exception& e){if(!state.empty())status(state,e.what());}
    if(parent)CloseHandle(parent);return 1;
}
