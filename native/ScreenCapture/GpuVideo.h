#pragma once
#include <windows.h>
#include <d3d11_4.h>
#include <dxgi1_2.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <mftransform.h>
#include <evr.h>
#include <wrl/client.h>
#include <winrt/base.h>
#include <array>
#include <atomic>
#include <memory>
#include <string>
#include "CapturePolicy.h"

namespace fengyin::screen {
using Microsoft::WRL::ComPtr;
inline void checked(HRESULT hr){winrt::check_hresult(hr);}
inline void checked(HRESULT hr,const char* operation){
    if(FAILED(hr))throw std::runtime_error(std::string(operation)+": "+winrt::to_string(winrt::hresult_error(hr).message()));
}
inline std::int64_t qpc(){LARGE_INTEGER t,f;QueryPerformanceCounter(&t);QueryPerformanceFrequency(&f);return std::int64_t(double(t.QuadPart)*second/f.QuadPart);}
struct Slots { std::array<std::atomic<bool>,4> busy{}; };
// IMFTrackedSample returns the surface only after the encoder releases it.
class Recycle final : public IMFAsyncCallback {
    std::atomic<ULONG> refs{1};std::shared_ptr<Slots> slots;std::size_t slot;
public:
    Recycle(std::shared_ptr<Slots> p,std::size_t i):slots(std::move(p)),slot(i){}
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID id,void** out) override {
        if(!out)return E_POINTER;*out=nullptr;
        if(id==__uuidof(IUnknown)||id==__uuidof(IMFAsyncCallback)){*out=static_cast<IMFAsyncCallback*>(this);AddRef();return S_OK;}return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef() override{return ++refs;}
    ULONG STDMETHODCALLTYPE Release() override{auto n=--refs;if(!n)delete this;return n;}
    HRESULT STDMETHODCALLTYPE GetParameters(DWORD*,DWORD*) override{return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE Invoke(IMFAsyncResult*) override{slots->busy[slot].store(false,std::memory_order_release);return S_OK;}
};

class GpuVideo {
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
    ComPtr<ID3D11VideoDevice> videoDevice;
    ComPtr<ID3D11VideoContext> videoContext;
    ComPtr<ID3D11VideoProcessorEnumerator> enumerator;
    ComPtr<ID3D11VideoProcessor> processor;
    ComPtr<IMFDXGIDeviceManager> manager;
    ComPtr<IMFSinkWriter> writer;
    std::array<ComPtr<ID3D11Texture2D>,4> textures;
    std::array<ComPtr<ID3D11VideoProcessorOutputView>,4> views;
    std::shared_ptr<Slots> slots=std::make_shared<Slots>();
    int sourceW=0,sourceH=0;Size output{};DWORD stream=0;
public:
    std::string encoder;
    FrameHealth health;
    ID3D11Device* getDevice() const{return device.Get();}
    void initialise(int w,int h,bool allowWarpForConversionTest=false){
        sourceW=w;sourceH=h;output=canvas(w,h);
        auto hr=D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,
            D3D11_CREATE_DEVICE_BGRA_SUPPORT|D3D11_CREATE_DEVICE_VIDEO_SUPPORT,nullptr,0,D3D11_SDK_VERSION,&device,nullptr,&context);
        if(FAILED(hr)&&allowWarpForConversionTest)hr=D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,
            D3D11_CREATE_DEVICE_BGRA_SUPPORT|D3D11_CREATE_DEVICE_VIDEO_SUPPORT,nullptr,0,D3D11_SDK_VERSION,&device,nullptr,&context);
        checked(hr,"Create D3D11 video device");
        // Use the Windows 10 D3D11 interface on its documented immediate
        // context, not the D3D10 compatibility interface on a D3D11 object.
        ComPtr<ID3D11Multithread> multithread;checked(context.As(&multithread),"D3D11 context multithread interface");multithread->SetMultithreadProtected(TRUE);
        checked(device.As(&videoDevice),"D3D11 video device interface");checked(context.As(&videoContext),"D3D11 video context interface");
        UINT token=0;checked(MFCreateDXGIDeviceManager(&token,&manager));checked(manager->ResetDevice(device.Get(),token));
        D3D11_VIDEO_PROCESSOR_CONTENT_DESC content{};content.InputFrameFormat=D3D11_VIDEO_FRAME_FORMAT_PROGRESSIVE;
        content.InputWidth=w;content.InputHeight=h;content.OutputWidth=output.width;content.OutputHeight=output.height;
        content.InputFrameRate={fps,1};content.OutputFrameRate={fps,1};content.Usage=D3D11_VIDEO_USAGE_PLAYBACK_NORMAL;
        checked(videoDevice->CreateVideoProcessorEnumerator(&content,&enumerator),"Create GPU video processor enumerator");
        UINT flags=0;checked(enumerator->CheckVideoProcessorFormat(DXGI_FORMAT_B8G8R8A8_UNORM,&flags));
        if(!(flags&D3D11_VIDEO_PROCESSOR_FORMAT_SUPPORT_INPUT))throw std::runtime_error("GPU cannot process BGRA capture");
        checked(enumerator->CheckVideoProcessorFormat(DXGI_FORMAT_NV12,&flags));
        if(!(flags&D3D11_VIDEO_PROCESSOR_FORMAT_SUPPORT_OUTPUT))throw std::runtime_error("GPU cannot produce NV12");
        checked(videoDevice->CreateVideoProcessor(enumerator.Get(),0,&processor),"Create GPU video processor");
        RECT src{0,0,w,h},dst{0,0,output.width,output.height};
        videoContext->VideoProcessorSetStreamSourceRect(processor.Get(),0,TRUE,&src);
        videoContext->VideoProcessorSetStreamDestRect(processor.Get(),0,TRUE,&dst);
        videoContext->VideoProcessorSetOutputTargetRect(processor.Get(),TRUE,&dst);
        videoContext->VideoProcessorSetStreamFrameFormat(processor.Get(),0,D3D11_VIDEO_FRAME_FORMAT_PROGRESSIVE);
        videoContext->VideoProcessorSetStreamAutoProcessingMode(processor.Get(),0,FALSE);
        D3D11_VIDEO_PROCESSOR_COLOR_SPACE rgb{};rgb.RGB_Range=0;
        D3D11_VIDEO_PROCESSOR_COLOR_SPACE yuv{};yuv.YCbCr_Matrix=1;yuv.Nominal_Range=1;
        videoContext->VideoProcessorSetStreamColorSpace(processor.Get(),0,&rgb);
        videoContext->VideoProcessorSetOutputColorSpace(processor.Get(),&yuv);
        for(std::size_t i=0;i<textures.size();++i){
            D3D11_TEXTURE2D_DESC desc{};desc.Width=output.width;desc.Height=output.height;desc.MipLevels=1;desc.ArraySize=1;
            desc.Format=DXGI_FORMAT_NV12;desc.SampleDesc.Count=1;desc.Usage=D3D11_USAGE_DEFAULT;desc.BindFlags=D3D11_BIND_RENDER_TARGET;
            checked(device->CreateTexture2D(&desc,nullptr,&textures[i]));
            D3D11_VIDEO_PROCESSOR_OUTPUT_VIEW_DESC view{};view.ViewDimension=D3D11_VPOV_DIMENSION_TEXTURE2D;
            checked(videoDevice->CreateVideoProcessorOutputView(textures[i].Get(),enumerator.Get(),&view,&views[i]));
        }
    }
    void open(const std::wstring& path){
        ComPtr<IMFAttributes> attrs;checked(MFCreateAttributes(&attrs,4));
        checked(attrs->SetUnknown(MF_SINK_WRITER_D3D_MANAGER,manager.Get()));
        checked(attrs->SetUINT32(MF_READWRITE_ENABLE_HARDWARE_TRANSFORMS,TRUE));
        checked(attrs->SetUINT32(MF_SINK_WRITER_DISABLE_THROTTLING,FALSE));
        checked(MFCreateSinkWriterFromURL(path.c_str(),nullptr,attrs.Get(),&writer));
        ComPtr<IMFMediaType> out,in;checked(MFCreateMediaType(&out));
        checked(out->SetGUID(MF_MT_MAJOR_TYPE,MFMediaType_Video));checked(out->SetGUID(MF_MT_SUBTYPE,MFVideoFormat_H264));
        checked(out->SetUINT32(MF_MT_AVG_BITRATE,bitrate));checked(out->SetUINT32(MF_MT_INTERLACE_MODE,MFVideoInterlace_Progressive));
        checked(MFSetAttributeSize(out.Get(),MF_MT_FRAME_SIZE,output.width,output.height));
        checked(MFSetAttributeRatio(out.Get(),MF_MT_FRAME_RATE,fps,1));checked(MFSetAttributeRatio(out.Get(),MF_MT_PIXEL_ASPECT_RATIO,1,1));
        checked(writer->AddStream(out.Get(),&stream));checked(MFCreateMediaType(&in));checked(out->CopyAllItems(in.Get()));
        checked(in->SetGUID(MF_MT_SUBTYPE,MFVideoFormat_NV12));checked(in->SetUINT32(MF_MT_DEFAULT_STRIDE,output.width));
        checked(in->SetUINT32(MF_MT_YUV_MATRIX,MFVideoTransferMatrix_BT709));
        checked(in->SetUINT32(MF_MT_VIDEO_NOMINAL_RANGE,MFNominalRange_16_235));
        checked(writer->SetInputMediaType(stream,in.Get(),nullptr),"Configure NV12 hardware encoder input");
        // Windows 10-compatible actual encoder inspection. Merely enabling HW
        // transforms is not evidence; the Windows 11 25H2-only HW-only flag is
        // deliberately not used. Never feed frames to an unproven encoder.
        ComPtr<IMFSinkWriterEx> ex;checked(writer.As(&ex));bool proven=false;
        for(DWORD n=0;n<16;++n){
            GUID category{};ComPtr<IMFTransform> transform;
            if(FAILED(ex->GetTransformForStream(stream,n,&category,&transform)))break;
            if(category!=MFT_CATEGORY_VIDEO_ENCODER)continue;
            ComPtr<IMFAttributes> a;checked(transform->GetAttributes(&a));MF_ATTRIBUTE_TYPE type{};
            UINT32 d3d11=0;
            proven=SUCCEEDED(a->GetItemType(MFT_ENUM_HARDWARE_URL_Attribute,&type))
                && SUCCEEDED(a->GetUINT32(MF_SA_D3D11_AWARE,&d3d11)) && d3d11!=0;
            WCHAR name[256]{};UINT32 length=0;
            encoder=SUCCEEDED(a->GetString(MFT_FRIENDLY_NAME_Attribute,name,256,&length))?winrt::to_string(name):"hardware encoder";
            break;
        }
        if(!proven){writer.Reset();throw std::runtime_error("No confirmed GPU hardware H264 encoder; software fallback prohibited");}
        checked(writer->BeginWriting());
    }
    void convert(ID3D11Texture2D* source,std::size_t slot){
        D3D11_TEXTURE2D_DESC desc{};source->GetDesc(&desc);
        if(int(desc.Width)<sourceW||int(desc.Height)<sourceH)throw std::runtime_error("Capture surface dimensions changed");
        D3D11_VIDEO_PROCESSOR_INPUT_VIEW_DESC vd{};vd.ViewDimension=D3D11_VPIV_DIMENSION_TEXTURE2D;
        ComPtr<ID3D11VideoProcessorInputView> input;checked(videoDevice->CreateVideoProcessorInputView(source,enumerator.Get(),&vd,&input));
        D3D11_VIDEO_PROCESSOR_STREAM frame{};frame.Enable=TRUE;frame.pInputSurface=input.Get();
        checked(videoContext->VideoProcessorBlt(processor.Get(),views.at(slot).Get(),0,1,&frame));
        context->Flush(); // Submit GPU work; not a CPU readback or GPU wait.
    }
    void submit(ID3D11Texture2D* source,std::int64_t time){
        std::size_t slot=0;for(;slot<textures.size();++slot){bool free=false;if(slots->busy[slot].compare_exchange_strong(free,true))break;}
        if(slot==textures.size())throw std::runtime_error("Encoder retained all four GPU surfaces; recording stopped, not silently degraded");
        bool tracked=false;
        try{
            health.observe(time);convert(source,slot);
            ComPtr<IMFSample> sample;checked(MFCreateVideoSampleFromSurface(nullptr,&sample));
            ComPtr<IMFMediaBuffer> buffer;checked(MFCreateDXGISurfaceBuffer(__uuidof(ID3D11Texture2D),textures[slot].Get(),0,FALSE,&buffer));
            checked(sample->AddBuffer(buffer.Get()));checked(sample->SetSampleTime(time));checked(sample->SetSampleDuration(second/fps));
            ComPtr<IMFTrackedSample> tracking;checked(sample.As(&tracking));
            ComPtr<IMFAsyncCallback> callback;callback.Attach(new Recycle(slots,slot));checked(tracking->SetAllocator(callback.Get(),nullptr));tracked=true;
            checked(writer->WriteSample(stream,sample.Get()));
        }catch(...){if(!tracked)slots->busy[slot].store(false);throw;}
    }
    void close(){if(writer){auto hr=writer->Finalize();writer.Reset();checked(hr);}}
    ~GpuVideo(){try{close();}catch(...){}}
};
}
