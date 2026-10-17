// Windows-only window capture + default-speaker loopback + MP4 writer.
// No desktop/screen fallback, microphone, driver installation or audio routing.
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include <dwmapi.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <windows.graphics.capture.interop.h>
#include <windows.graphics.directx.direct3d11.interop.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Metadata.h>
#include <winrt/Windows.Graphics.Capture.h>
#include <winrt/Windows.Graphics.DirectX.Direct3D11.h>
#include <winrt/Windows.Graphics.DirectX.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <mferror.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <wrl/client.h>
#include <filesystem>
#include <fstream>
#include <vector>
#include <algorithm>
#include <cmath>
#include <chrono>
#include <string>
#include <thread>
#include <mutex>
#include <deque>
#include <atomic>
#include <future>
#include "AudioTimeline.h"
using Microsoft::WRL::ComPtr;
using namespace winrt;
using namespace winrt::Windows::Graphics::Capture;
using namespace winrt::Windows::Graphics::DirectX;
using namespace winrt::Windows::Graphics::DirectX::Direct3D11;
using namespace std::chrono;

static void check(HRESULT hr){winrt::check_hresult(hr);}
static bool status(const std::filesystem::path& path,const std::string& text) {
    std::ofstream out(path,std::ios::binary|std::ios::trunc);out<<text;out.flush();return bool(out);
}
static LONGLONG ticks(){LARGE_INTEGER q,f;QueryPerformanceCounter(&q);QueryPerformanceFrequency(&f);
    return static_cast<LONGLONG>(q.QuadPart*10000000.0/f.QuadPart);}

class Writer {
    ComPtr<IMFSinkWriter> writer;
    DWORD video=0,audio=0;
public:
    int width=0,height=0;
    void open(const wchar_t* path,int w,int h) {
        width=w;height=h;
        ComPtr<IMFAttributes> attributes;check(MFCreateAttributes(&attributes,3));
        check(attributes->SetUINT32(MF_READWRITE_ENABLE_HARDWARE_TRANSFORMS,TRUE));
        // Let the writer back-pressure this helper, not accumulate an unbounded
        // encode queue. Late video is dropped by the wall-clock loop below.
        check(attributes->SetUINT32(MF_SINK_WRITER_DISABLE_THROTTLING,FALSE));
        check(MFCreateSinkWriterFromURL(path,nullptr,attributes.Get(),&writer));
        ComPtr<IMFMediaType> out,in;
        check(MFCreateMediaType(&out));check(out->SetGUID(MF_MT_MAJOR_TYPE,MFMediaType_Video));
        check(out->SetGUID(MF_MT_SUBTYPE,MFVideoFormat_H264));
        check(out->SetUINT32(MF_MT_AVG_BITRATE,4000000));
        check(out->SetUINT32(MF_MT_INTERLACE_MODE,MFVideoInterlace_Progressive));
        check(MFSetAttributeSize(out.Get(),MF_MT_FRAME_SIZE,w,h));
        check(MFSetAttributeRatio(out.Get(),MF_MT_FRAME_RATE,24,1));
        check(MFSetAttributeRatio(out.Get(),MF_MT_PIXEL_ASPECT_RATIO,1,1));
        check(writer->AddStream(out.Get(),&video));
        check(MFCreateMediaType(&in));check(in->SetGUID(MF_MT_MAJOR_TYPE,MFMediaType_Video));
        check(in->SetGUID(MF_MT_SUBTYPE,MFVideoFormat_RGB32));
        check(in->SetUINT32(MF_MT_INTERLACE_MODE,MFVideoInterlace_Progressive));
        check(in->SetUINT32(MF_MT_DEFAULT_STRIDE,w*4));
        check(MFSetAttributeSize(in.Get(),MF_MT_FRAME_SIZE,w,h));
        check(MFSetAttributeRatio(in.Get(),MF_MT_FRAME_RATE,24,1));
        check(MFSetAttributeRatio(in.Get(),MF_MT_PIXEL_ASPECT_RATIO,1,1));
        check(writer->SetInputMediaType(video,in.Get(),nullptr));
        out.Reset();in.Reset();check(MFCreateMediaType(&out));
        check(out->SetGUID(MF_MT_MAJOR_TYPE,MFMediaType_Audio));
        check(out->SetGUID(MF_MT_SUBTYPE,MFAudioFormat_AAC));
        check(out->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS,2));
        check(out->SetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND,48000));
        check(out->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE,16));
        check(out->SetUINT32(MF_MT_AUDIO_AVG_BYTES_PER_SECOND,24000));
        check(out->SetUINT32(MF_MT_AAC_PAYLOAD_TYPE,0));
        check(writer->AddStream(out.Get(),&audio));
        check(MFCreateMediaType(&in));check(in->SetGUID(MF_MT_MAJOR_TYPE,MFMediaType_Audio));
        check(in->SetGUID(MF_MT_SUBTYPE,MFAudioFormat_PCM));
        check(in->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS,2));
        check(in->SetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND,48000));
        check(in->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE,16));
        check(in->SetUINT32(MF_MT_AUDIO_BLOCK_ALIGNMENT,4));
        check(in->SetUINT32(MF_MT_AUDIO_AVG_BYTES_PER_SECOND,192000));
        check(writer->SetInputMediaType(audio,in.Get(),nullptr));
        check(writer->BeginWriting());
    }
    void sample(DWORD stream,const void* data,DWORD size,LONGLONG time,LONGLONG duration) {
        ComPtr<IMFMediaBuffer> buffer;check(MFCreateMemoryBuffer(size,&buffer));
        BYTE* dst=nullptr;check(buffer->Lock(&dst,nullptr,nullptr));memcpy(dst,data,size);
        check(buffer->Unlock());check(buffer->SetCurrentLength(size));
        ComPtr<IMFSample> s;check(MFCreateSample(&s));check(s->AddBuffer(buffer.Get()));
        check(s->SetSampleTime(time));check(s->SetSampleDuration(duration));
        check(writer->WriteSample(stream,s.Get()));
    }
    void image(const std::vector<uint32_t>& data,LONGLONG time,LONGLONG duration) {
        sample(video,data.data(),static_cast<DWORD>(data.size()*4),time,duration);
    }
    void sound(const std::vector<int16_t>& data,LONGLONG frame) {
        sample(audio,data.data(),static_cast<DWORD>(data.size()*2),frame*10000000/48000,
               static_cast<LONGLONG>(data.size()/2)*10000000/48000);
    }
    void close(){if(writer){check(writer->Finalize());writer.Reset();}}
};

struct AudioPacket {LONGLONG first=0;std::vector<int16_t> pcm;};
class Loopback {
    ComPtr<IMMDeviceEnumerator> enumerator;
    ComPtr<IMMDevice> device;
    ComPtr<IAudioClient> client;
    ComPtr<IAudioCaptureClient> capture;
    std::wstring endpoint;
public:
    LONGLONG startQpc=0;
    CaptureAudioTimeline timeline;
    void open() {
        check(CoCreateInstance(__uuidof(MMDeviceEnumerator),nullptr,CLSCTX_ALL,IID_PPV_ARGS(&enumerator)));
        check(enumerator->GetDefaultAudioEndpoint(eRender,eMultimedia,&device));
        LPWSTR id=nullptr;check(device->GetId(&id));endpoint=id;CoTaskMemFree(id);
        check(device->Activate(__uuidof(IAudioClient),CLSCTX_ALL,nullptr,reinterpret_cast<void**>(client.GetAddressOf())));
        WAVEFORMATEX format {WAVE_FORMAT_IEEE_FLOAT,2,48000,48000*8,8,32,0};
        check(client->Initialize(AUDCLNT_SHAREMODE_SHARED,AUDCLNT_STREAMFLAGS_LOOPBACK
            |AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM|AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY,
            1000000,0,&format,nullptr));
        check(client->GetService(IID_PPV_ARGS(&capture)));
    }
    void start(LONGLONG qpc){startQpc=qpc;check(client->Start());}
    std::vector<AudioPacket> drain() {
        std::vector<AudioPacket> packets;
        UINT32 available=0;check(capture->GetNextPacketSize(&available));
        while(available) {
            BYTE* data=nullptr;UINT32 frames=0;DWORD flags=0;UINT64 devicePosition=0,qpc=0;
            check(capture->GetBuffer(&data,&frames,&flags,&devicePosition,&qpc));
            // QPC is in 100ns units; align real speaker packets with video.
            const auto packetQpc=static_cast<LONGLONG>(qpc);
            const bool validTime=!(flags&AUDCLNT_BUFFERFLAGS_TIMESTAMP_ERROR)
                && std::abs(packetQpc-ticks())<20000000;
            const LONGLONG first=timeline.packet(devicePosition,packetQpc,startQpc,validTime,frames);
            std::vector<int16_t> pcm(static_cast<size_t>(frames)*2,0);
            if(!(flags&AUDCLNT_BUFFERFLAGS_SILENT) && data) {
                const auto* f=reinterpret_cast<const float*>(data);
                for(size_t i=0;i<pcm.size();++i) {
                    const auto v=f[i];
                    pcm[i]=static_cast<int16_t>(std::lround(std::clamp(std::isfinite(v)?v:0.f,-1.f,1.f)*32767.f));
                }
            }
            check(capture->ReleaseBuffer(frames));
            packets.push_back({first,std::move(pcm)});
            check(capture->GetNextPacketSize(&available));
        }
        return packets;
    }
    bool sameEndpoint() {
        ComPtr<IMMDevice> now;check(enumerator->GetDefaultAudioEndpoint(eRender,eMultimedia,&now));
        LPWSTR id=nullptr;check(now->GetId(&id));const bool same=endpoint==id;CoTaskMemFree(id);return same;
    }
    ~Loopback(){if(client)client->Stop();}
};

// Capture owns its COM interfaces on one dedicated thread. GPU readback and
// encoder back-pressure must never hold an IAudioCaptureClient packet open.
class SpeakerCapture {
    std::thread worker;
    std::atomic<bool> stopping{false};
    std::mutex mutex;
    std::deque<AudioPacket> queue;
    size_t queuedFrames=0;
    std::exception_ptr failure;
    LONGLONG capturedThrough=0,written=0;
    void append(std::vector<AudioPacket> packets,LONGLONG watermark) {
        std::lock_guard<std::mutex> lock(mutex);
        for(auto& packet:packets) {
            queuedFrames+=packet.pcm.size()/2;
            if(queuedFrames>48000*10)throw hresult_error(E_FAIL,L"Audio encoder cannot keep up; capture queue exceeded ten seconds");
            queue.push_back(std::move(packet));
        }
        capturedThrough=watermark;
    }
    void silence(Writer& writer,LONGLONG target) {
        while(written<target) {
            const auto n=static_cast<size_t>(std::min<LONGLONG>(target-written,4800));
            writer.sound(std::vector<int16_t>(n*2,0),written);written+=n;
        }
    }
public:
    void start(LONGLONG origin) {
        std::promise<void> ready;auto future=ready.get_future();
        worker=std::thread([this,origin,ready=std::move(ready)]() mutable {
            bool announced=false;
            try {
                struct Apartment {
                    Apartment(){winrt::init_apartment(winrt::apartment_type::multi_threaded);}
                    ~Apartment(){winrt::uninit_apartment();}
                } apartment;
                {
                    Loopback loop;loop.open();loop.start(origin);
                    ready.set_value();announced=true;
                    auto endpointCheck=steady_clock::now();
                    while(!stopping.load()) {
                        auto packets=loop.drain();
                        // Only advance silence behind a successfully drained
                        // capture worker, never from a stalled video/main loop.
                        append(std::move(packets),std::max<LONGLONG>(0,(ticks()-origin-2000000)*48000/10000000));
                        if(steady_clock::now()-endpointCheck>seconds(1)) {
                            if(!loop.sameEndpoint())throw hresult_error(AUDCLNT_E_DEVICE_INVALIDATED,L"Speaker changed; recording stopped");
                            endpointCheck=steady_clock::now();
                        }
                        Sleep(3);
                    }
                    append(loop.drain(),std::max<LONGLONG>(0,(ticks()-origin)*48000/10000000));
                }
            } catch(...) {
                auto error=std::current_exception();
                {std::lock_guard<std::mutex> lock(mutex);failure=error;}
                if(!announced)ready.set_exception(error);
            }
        });
        future.get();
    }
    void drain(Writer& writer) {
        std::deque<AudioPacket> packets;LONGLONG watermark=0;std::exception_ptr error;
        {
            std::lock_guard<std::mutex> lock(mutex);
            packets.swap(queue);queuedFrames=0;watermark=capturedThrough;error=failure;
        }
        if(error)std::rethrow_exception(error);
        for(auto& p:packets) {
            if(p.first>written)silence(writer,p.first);
            const auto skip=static_cast<size_t>(std::clamp<LONGLONG>(written-p.first,0,p.pcm.size()/2));
            if(skip)p.pcm.erase(p.pcm.begin(),p.pcm.begin()+static_cast<ptrdiff_t>(skip*2));
            if(!p.pcm.empty()){writer.sound(p.pcm,written);written+=p.pcm.size()/2;}
        }
        silence(writer,watermark);
    }
    void stop(){stopping.store(true);if(worker.joinable())worker.join();}
    ~SpeakerCapture(){stop();}
};

class WindowFrames {
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
    ComPtr<ID3D11Texture2D> staging;
    IDirect3DDevice projected{nullptr};
    GraphicsCaptureItem item{nullptr};
    Direct3D11CaptureFramePool pool{nullptr};
    GraphicsCaptureSession session{nullptr};
    winrt::event_token token{};
    int sourceW=0,sourceH=0,poolW=0,poolH=0;
public:
    void open(HWND hwnd) {
        if(!GraphicsCaptureSession::IsSupported())throw hresult_error(E_NOTIMPL,L"Windows window capture unavailable");
        check(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,D3D11_CREATE_DEVICE_BGRA_SUPPORT,
            nullptr,0,D3D11_SDK_VERSION,&device,nullptr,&context));
        ComPtr<IDXGIDevice> dxgi;check(device.As(&dxgi));
        ComPtr<::IInspectable> inspectable;check(CreateDirect3D11DeviceFromDXGIDevice(dxgi.Get(),&inspectable));
        projected=winrt::com_ptr<::IInspectable>(inspectable.Detach(),winrt::take_ownership_from_abi).as<IDirect3DDevice>();
        auto interop=winrt::get_activation_factory<GraphicsCaptureItem,IGraphicsCaptureItemInterop>();
        check(interop->CreateForWindow(hwnd,winrt::guid_of<GraphicsCaptureItem>(),winrt::put_abi(item)));
        pool=Direct3D11CaptureFramePool::CreateFreeThreaded(projected,DirectXPixelFormat::B8G8R8A8UIntNormalized,2,item.Size());
        poolW=item.Size().Width;poolH=item.Size().Height;
        token=pool.FrameArrived([](auto const&,auto const&){});
        session=pool.CreateCaptureSession(item);
        if(winrt::Windows::Foundation::Metadata::ApiInformation::IsPropertyPresent(
            L"Windows.Graphics.Capture.GraphicsCaptureSession",L"IsCursorCaptureEnabled"))session.IsCursorCaptureEnabled(false);
        session.StartCapture();
    }
    bool read(HWND hwnd,std::vector<uint32_t>& pixels,int w,int h,uint32_t bg,bool rounded) {
        auto frame=pool.TryGetNextFrame();if(!frame)return false;
        const auto size=frame.ContentSize();if(size.Width<=0||size.Height<=0)return false;
        auto access=frame.Surface().as<::Windows::Graphics::DirectX::Direct3D11::IDirect3DDxgiInterfaceAccess>();
        ComPtr<ID3D11Texture2D> texture;check(access->GetInterface(IID_PPV_ARGS(&texture)));
        D3D11_TEXTURE2D_DESC desc{};texture->GetDesc(&desc);
        if(!staging||sourceW!=static_cast<int>(desc.Width)||sourceH!=static_cast<int>(desc.Height)) {
            sourceW=desc.Width;sourceH=desc.Height;desc.Usage=D3D11_USAGE_STAGING;
            desc.BindFlags=0;desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;desc.MiscFlags=0;
            staging.Reset();check(device->CreateTexture2D(&desc,nullptr,&staging));
        }
        context->CopyResource(staging.Get(),texture.Get());
        D3D11_MAPPED_SUBRESOURCE mapped{};check(context->Map(staging.Get(),0,D3D11_MAP_READ,0,&mapped));
        RECT bounds{},client{};GetClientRect(hwnd,&client);POINT origin{0,0};ClientToScreen(hwnd,&origin);
        if(FAILED(DwmGetWindowAttribute(hwnd,DWMWA_EXTENDED_FRAME_BOUNDS,&bounds,sizeof(bounds))))GetWindowRect(hwnd,&bounds);
        const int left=std::clamp(static_cast<int>(origin.x-bounds.left),0,sourceW-1);
        const int top=std::clamp(static_cast<int>(origin.y-bounds.top),0,sourceH-1);
        const int cw=std::min({static_cast<int>(client.right),size.Width-left,sourceW-left});
        const int ch=std::min({static_cast<int>(client.bottom),size.Height-top,sourceH-top});
        pixels.assign(static_cast<size_t>(w)*h,bg);
        if(cw>0&&ch>0) {
            const double scale=std::min(double(w)/cw,double(h)/ch);
            const int dw=std::max(1,int(cw*scale)),dh=std::max(1,int(ch*scale));
            const int ox=(w-dw)/2,oy=(h-dh)/2;
            const double radius=rounded?28.0*GetDpiForWindow(hwnd)/96.0*scale:0;
            for(int y=0;y<dh;++y)for(int x=0;x<dw;++x) {
                const double dx=std::max({radius-x-.5,0.0,x+.5-(dw-radius)});
                const double dy=std::max({radius-y-.5,0.0,y+.5-(dh-radius)});
                if(rounded&&dx*dx+dy*dy>radius*radius)continue;
                const int sy=top+std::min(ch-1,int(y/scale)),sx=left+std::min(cw-1,int(x/scale));
                const auto* row=reinterpret_cast<const uint32_t*>(static_cast<const BYTE*>(mapped.pData)+sy*mapped.RowPitch);
                pixels[static_cast<size_t>(oy+y)*w+ox+x]=row[sx]|0xff000000;
            }
        }
        context->Unmap(staging.Get(),0);frame.Close();
        // Capture surface size can change during resize, output canvas stays fixed.
        if(size.Width!=poolW || size.Height!=poolH) {
            pool.Recreate(projected,DirectXPixelFormat::B8G8R8A8UIntNormalized,2,size);
            poolW=size.Width;poolH=size.Height;
        }
        return cw>0&&ch>0;
    }
    ~WindowFrames(){if(session)session.Close();if(pool){pool.FrameArrived(token);pool.Close();}}
};

int WINAPI wWinMain(HINSTANCE,HINSTANCE,PWSTR,int) {
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    int argc=0;auto argv=CommandLineToArgvW(GetCommandLineW(),&argc);
    if(argc==3 && std::wstring(argv[1])==L"--selftest") {
        const std::filesystem::path output(argv[2]);LocalFree(argv);
        try {
            winrt::init_apartment(winrt::apartment_type::multi_threaded);check(MFStartup(MF_VERSION));
            Writer writer;writer.open(output.c_str(),320,240);
            std::vector<uint32_t> image(320*240);
            for(int y=0;y<240;++y)for(int x=0;x<320;++x)image[y*320+x]=y<120?0xffff0000:0xff0000ff;
            CaptureAudioTimeline timeline;
            for(int frame=0;frame<48;++frame) {
                writer.image(image,frame*10000000LL/24,10000000/24);
                std::vector<int16_t> audio(2000*2);
                for(int i=0;i<2000;++i)audio[i*2]=audio[i*2+1]=static_cast<int16_t>(3000*std::sin((frame*2000+i)*6.283185307179586*440/48000));
                const auto jitter=frame?((frame%3)-1)*20000LL:0;
                const auto first=timeline.packet(static_cast<uint64_t>(frame)*2000,frame*2000LL*10000000/48000+jitter,0,true,2000);
                if(first!=frame*2000LL)throw hresult_error(E_FAIL,L"Audio timeline inserted a discontinuity");
                writer.sound(audio,first);
            }
            writer.close();MFShutdown();return 0;
        }catch(const winrt::hresult_error& e){
            status(output.wstring()+L".error.txt",winrt::to_string(e.message())+" ("+std::to_string(static_cast<long>(e.code()))+")");
            MFShutdown();return 4;
        }catch(const std::exception& e){status(output.wstring()+L".error.txt",e.what());MFShutdown();return 4;}
    }
    if(argc!=7){if(argv)LocalFree(argv);return 2;}
    const auto hwnd=reinterpret_cast<HWND>(static_cast<uintptr_t>(std::stoull(argv[1],nullptr,16)));
    const std::filesystem::path output(argv[2]),stop(argv[3]),state(argv[4]);
    const auto color=static_cast<uint32_t>(std::stoul(argv[5],nullptr,16));const bool rounded=std::wstring(argv[6])==L"1";
    LocalFree(argv);bool mfStarted=false;
    Writer writer;
    try {
        winrt::init_apartment(winrt::apartment_type::multi_threaded);
        check(MFStartup(MF_VERSION));mfStarted=true;
        if(!IsWindow(hwnd))throw hresult_error(E_INVALIDARG,L"FengYin window unavailable");
        WindowFrames frames;frames.open(hwnd);SpeakerCapture sound;
        RECT rect{};GetClientRect(hwnd,&rect);
        const double scale=std::min({1.0,1280.0/std::max<LONG>(2,rect.right),720.0/std::max<LONG>(2,rect.bottom)});
        const int w=std::max(2,int(rect.right*scale)/2*2),h=std::max(2,int(rect.bottom*scale)/2*2);
        writer.open(output.c_str(),w,h);
        std::vector<uint32_t> pixels;const auto timeout=steady_clock::now()+seconds(8);
        while(!frames.read(hwnd,pixels,w,h,color,rounded)) {
            if(std::filesystem::exists(stop))throw hresult_error(E_ABORT,L"Recording cancelled during startup");
            if(steady_clock::now()>timeout)throw hresult_error(E_FAIL,L"No window capture frames");Sleep(10);
        }
        const auto origin=ticks();sound.start(origin);
        if(!status(state,"recording"))throw hresult_error(E_ACCESSDENIED,L"Cannot write recording status");
        LONGLONG next=0,frameCount=0;
        while(!std::filesystem::exists(stop)) {
            if(!IsWindow(hwnd)||IsIconic(hwnd)||!IsWindowVisible(hwnd))break;
            sound.drain(writer);
            const auto now=ticks()-origin;
            if(now>=next) {
                frames.read(hwnd,pixels,w,h,color,rounded);
                // Do not queue unbounded video frames if encoding falls behind.
                writer.image(pixels,now,10000000/24);++frameCount;next=now+10000000/24;
            }
            Sleep(4);
        }
        sound.stop();sound.drain(writer);
        writer.close();const bool reported=status(state,frameCount>0?"saved":"No encoded video frames");
        MFShutdown();return frameCount>0&&reported?0:3;
    } catch(const winrt::hresult_error& e) {
        try{writer.close();}catch(...){}
        status(state,"Windows capture error "+winrt::to_string(e.message())+" ("+std::to_string(static_cast<long>(e.code()))+")");
    } catch(const std::exception& e) {
        try{writer.close();}catch(...){}status(state,std::string("Capture error: ")+e.what());
    }
    if(mfStarted)MFShutdown();return 1;
}
