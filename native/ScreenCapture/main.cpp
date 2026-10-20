#include "GpuVideo.h"
#include "TimedAudio.h"
#include "OfflineMux.h"
#include <windows.graphics.capture.interop.h>
#include <windows.graphics.directx.direct3d11.interop.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Graphics.Capture.h>
#include <winrt/Windows.Graphics.DirectX.h>
#include <winrt/Windows.Graphics.DirectX.Direct3D11.h>
#include <fstream>
#include <iostream>
#include <vector>
#include <cmath>
using namespace fengyin::screen;
using namespace winrt::Windows::Graphics::Capture;
using namespace winrt::Windows::Graphics::DirectX;
using namespace winrt::Windows::Graphics::DirectX::Direct3D11;

static void status(const std::filesystem::path& folder,const std::string& value){
    const auto temporary=folder/L"state.tmp",target=folder/L"state.txt";
    {std::ofstream out(temporary);out<<value;out.flush();if(!out)throw std::runtime_error("Cannot write session state");}
    if(!MoveFileExW(temporary.c_str(),target.c_str(),MOVEFILE_REPLACE_EXISTING))throw std::runtime_error("Cannot publish session state");
}
static ComPtr<ID3D11Texture2D> pattern(GpuVideo& gpu,int w,int h){
    std::vector<std::uint32_t> pixels(std::size_t(w)*h);
    for(int y=0;y<h;++y)std::fill_n(pixels.data()+std::size_t(y)*w,w,y<h/2?0xffff0000:0xff0000ff);
    D3D11_TEXTURE2D_DESC d{};d.Width=w;d.Height=h;d.MipLevels=1;d.ArraySize=1;
    d.Format=DXGI_FORMAT_B8G8R8A8_UNORM;d.SampleDesc.Count=1;d.BindFlags=D3D11_BIND_RENDER_TARGET|D3D11_BIND_SHADER_RESOURCE;
    D3D11_SUBRESOURCE_DATA data{pixels.data(),UINT(w*4),0};ComPtr<ID3D11Texture2D> texture;
    checked(gpu.getDevice()->CreateTexture2D(&d,&data,&texture));return texture;
}
static void record(HWND hwnd,const std::filesystem::path& folder,HANDLE parent,int maxSeconds){
    if(!IsWindow(hwnd))throw std::runtime_error("FengYin window is unavailable");
    const auto monitor=MonitorFromWindow(hwnd,MONITOR_DEFAULTTONEAREST);MONITORINFO info{};info.cbSize=sizeof(info);
    if(!GetMonitorInfoW(monitor,&info))throw std::runtime_error("Cannot find target monitor");
    const int width=info.rcMonitor.right-info.rcMonitor.left,height=info.rcMonitor.bottom-info.rcMonitor.top;
    // Deliberately full monitor, including taskbar: no taskbar special path.
    GpuVideo gpu;gpu.initialise(width,height);gpu.open((folder/L"video.partial.mp4").wstring());
    ComPtr<IDXGIDevice> dxgi;checked(gpu.getDevice()->QueryInterface(IID_PPV_ARGS(&dxgi)));
    ComPtr<::IInspectable> inspect;checked(CreateDirect3D11DeviceFromDXGIDevice(dxgi.Get(),&inspect));
    auto projected=winrt::com_ptr<::IInspectable>(inspect.Detach(),winrt::take_ownership_from_abi).as<IDirect3DDevice>();
    auto factory=winrt::get_activation_factory<GraphicsCaptureItem,IGraphicsCaptureItemInterop>();GraphicsCaptureItem item{nullptr};
    checked(factory->CreateForMonitor(monitor,winrt::guid_of<GraphicsCaptureItem>(),winrt::put_abi(item)));
    auto pool=Direct3D11CaptureFramePool::CreateFreeThreaded(projected,DirectXPixelFormat::B8G8R8A8UIntNormalized,2,item.Size());
    auto session=pool.CreateCaptureSession(item);
    auto audio=std::make_unique<TimedAudio>();audio->start(folder/L"audio.wav");
    auto deadline=qpc()+5*second;
    while(!audio->firstQpc()){audio->checkError();if(qpc()>deadline)throw std::runtime_error("No timestamped speaker samples");Sleep(5);}
    session.StartCapture();
    std::int64_t origin=0,lastFrame=0,next=0;auto checkAt=GetTickCount64()+1000;
    Direct3D11CaptureFrame held{nullptr};std::int64_t heldStamp=0;std::uint64_t reusedFrames=0;
    std::int64_t maxNewFrameAge=0;bool newSurface=false;
    const auto stop=[&]{return std::filesystem::exists(folder/L"stop")||WaitForSingleObject(parent,0)!=WAIT_TIMEOUT
        ||(origin&&qpc()-origin>=std::int64_t(maxSeconds)*second);};
    status(folder,"starting");
    try{
        deadline=qpc()+8*second;
        while(!stop()){
            audio->checkError();
            auto frame=pool.TryGetNextFrame();
            if(frame){
                for(int n=0;n<2;++n){auto newer=pool.TryGetNextFrame();if(!newer)break;frame.Close();frame=std::move(newer);}
                const auto size=frame.ContentSize();
                if(size.Width!=width||size.Height!=height)throw std::runtime_error("Display geometry changed; recording stopped");
                const auto time=frame.SystemRelativeTime().count();
                if(held)held.Close();held=std::move(frame);heldStamp=time;newSurface=true;
            }
            const auto now=qpc();
            if(held&&now>=next){
                    // WGC need not send unchanged desktop frames. Hold one
                    // frame-pool lease. Report every reuse separately: lack of
                    // a new surface alone does not prove the desktop was static.
                    const auto time=origin?now:heldStamp;
                    if(newSurface){maxNewFrameAge=std::max(maxNewFrameAge,now-heldStamp);
                        if(now-heldStamp>second/5)throw std::runtime_error("New capture frame is over 200ms stale");}
                    else ++reusedFrames;
                    auto access=held.Surface().as<::Windows::Graphics::DirectX::Direct3D11::IDirect3DDxgiInterfaceAccess>();
                    ComPtr<ID3D11Texture2D> texture;checked(access->GetInterface(IID_PPV_ARGS(&texture)));
                    if(!origin){origin=time;status(folder,"recording");}
                    gpu.submit(texture.Get(),time-origin);lastFrame=time;next=time+second/fps;newSurface=false;
            }else Sleep(1);
            if(!origin&&qpc()>deadline)throw std::runtime_error("No screen frames");
            if(origin&&qpc()-lastFrame>second/2)throw std::runtime_error("Capture stopped providing frames");
            if(GetTickCount64()>checkAt){MONITORINFO current{};current.cbSize=sizeof(current);
                if(!GetMonitorInfoW(monitor,&current)||!EqualRect(&info.rcMonitor,&current.rcMonitor))throw std::runtime_error("Display changed; recording stopped");
                if(std::filesystem::space(folder).available<256ULL*1024*1024)throw std::runtime_error("Low disk space; partial files retained");
                checkAt=GetTickCount64()+1000;
            }
        }
        if(held){held.Close();held=nullptr;}session.Close();pool.Close();
        // Capture the final audio block covering the last video frame.
        const auto end=lastFrame+second/fps,waitUntil=qpc()+second;
        while(audio->endQpc()<end&&qpc()<waitUntil){audio->checkError();Sleep(3);}
        audio->stop();audio->checkError();gpu.close();
        std::ofstream report(folder/L"metrics.txt");
        report<<"encoder="<<gpu.encoder<<"\nframes="<<gpu.health.frames<<"\nlateIntervals="<<gpu.health.late
              <<"\nmaxFrameGap100ns="<<gpu.health.maxGap<<"\nreusedSurfaceFrames="<<reusedFrames<<"\nmaxNewFrameAge100ns="<<maxNewFrameAge<<"\nvideoFirstQpc="<<origin<<"\naudioFirstQpc="<<audio->firstQpc()
              <<"\naudioEndQpc="<<audio->endQpc()<<"\naudioFrames="<<audio->frames()<<"\n";
        report.flush();if(!report)throw std::runtime_error("Cannot save session metrics");
        if(!origin)throw std::runtime_error("Recording cancelled before first frame");
        const auto duration=end-origin;
        const auto audioSpan=audio->endQpc()-audio->firstQpc();
        const auto sampleSpan=std::int64_t(audio->frames())*second/audioRate;
        if(std::abs(audioSpan-sampleSpan)>second/100)throw std::runtime_error("Audio clock drift exceeds 10ms; automatic mux withheld");
        if(origin<audio->firstQpc())throw std::runtime_error("Audio starts after video; automatic mux withheld");
        status(folder,"captured; run --mux-session after returning to idle");
        std::ofstream alignment(folder/L"alignment.txt");alignment<<origin-audio->firstQpc()<<" "<<duration<<"\n";
        alignment.flush();if(!alignment)throw std::runtime_error("Cannot save alignment");
    }catch(...){if(held)held.Close();session.Close();pool.Close();audio->stop();try{gpu.close();}catch(...){}throw;}
}
int wmain(int argc,wchar_t** argv){
    std::filesystem::path folder;
    try{
        SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
        struct MediaRuntime{
            MediaRuntime(){winrt::init_apartment(winrt::apartment_type::multi_threaded);
                const auto hr=MFStartup(MF_VERSION);if(FAILED(hr)){winrt::uninit_apartment();checked(hr);}}
            ~MediaRuntime(){MFShutdown();winrt::uninit_apartment();}
        } runtime;
        if(argc==2&&std::wstring(argv[1])==L"--gpu-selftest"){
            GpuVideo gpu;gpu.initialise(1920,1080,true);auto texture=pattern(gpu,1920,1080);
            for(int i=0;i<120;++i)gpu.convert(texture.Get(),i%4);
            std::cout<<"GPU BGRA->NV12 conversion submitted; not hardware encoder acceptance\n";return 0;
        }
        if(argc==3&&std::wstring(argv[1])==L"--encode-selftest"){
            if(std::filesystem::exists(argv[2]))throw std::runtime_error("Output exists");
            GpuVideo gpu;gpu.initialise(1920,1080);gpu.open(argv[2]);auto texture=pattern(gpu,1920,1080);
            const auto start=GetTickCount64();
            for(int n=0;n<60;++n){while(GetTickCount64()-start<std::uint64_t(n*1000/30))Sleep(1);gpu.submit(texture.Get(),n*second/fps);}
            gpu.close();std::cout<<"Confirmed encoder: "<<gpu.encoder<<"\n";return 0;
        }
        if(argc==7&&std::wstring(argv[1])==L"--mux-existing"){
            // CI/diagnostics only: independently generated fixtures, including
            // an explicit trim to exercise exact PCM sample alignment.
            mux(argv[2],argv[3],argv[4],std::stoll(argv[5]),std::stoll(argv[6]));return 0;
        }
        if(argc==3&&std::wstring(argv[1])==L"--mux-session"){
            folder=argv[2];std::ifstream alignment(folder/L"alignment.txt");std::int64_t trim=0,duration=0;
            if(!(alignment>>trim>>duration))throw std::runtime_error("Missing capture alignment");
            SetPriorityClass(GetCurrentProcess(),BELOW_NORMAL_PRIORITY_CLASS);
            status(folder,"finalizing");mux(folder/L"video.partial.mp4",folder/L"audio.wav",folder/L"recording.mp4",trim,duration);
            status(folder,"saved");return 0;
        }
        if((argc!=5&&argc!=6)||std::wstring(argv[1])!=L"--capture")throw std::runtime_error("Use --capture <HWND hex> <new session directory> <parent PID> [seconds]; diagnostic component, not production integration");
        const int seconds=argc==6?std::stoi(argv[5]):45;
        if(seconds<5||seconds>1800)throw std::runtime_error("Capture duration must be between 5 and 1800 seconds");
        folder=argv[3];if(!std::filesystem::create_directory(folder))throw std::runtime_error("Session directory must be new");
        if(std::filesystem::space(folder).available<1024ULL*1024*1024)throw std::runtime_error("At least 1GB free space required");
        HANDLE parent=OpenProcess(SYNCHRONIZE,FALSE,DWORD(std::stoul(argv[4])));
        if(!parent)throw std::runtime_error("Parent process not found");
        struct Close{HANDLE h;~Close(){CloseHandle(h);}} handle{parent};
        SetPriorityClass(GetCurrentProcess(),BELOW_NORMAL_PRIORITY_CLASS);
        record(reinterpret_cast<HWND>(std::uintptr_t(std::stoull(argv[2],nullptr,16))),folder,parent,seconds);
        return 0;
    }catch(const winrt::hresult_error& e){std::cerr<<winrt::to_string(e.message())<<" HRESULT="<<std::hex<<unsigned(e.code().value)<<"\n";
        if(!folder.empty()&&std::filesystem::exists(folder))try{status(folder,"failed: "+winrt::to_string(e.message()));}catch(...){}return 1;
    }catch(const std::exception& e){std::cerr<<e.what()<<"\n";
        if(!folder.empty()&&std::filesystem::exists(folder))try{status(folder,std::string("failed: ")+e.what());}catch(...){}return 1;
    }
}
