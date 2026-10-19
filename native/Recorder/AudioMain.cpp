// Release helper: miniaudio system-output WAV only. Video capture is archived.
#define NOMINMAX
#include <windows.h>
#include <shellapi.h>
#include <mmdeviceapi.h>
#include <wrl/client.h>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
#include <stdexcept>
#include "MiniAudioRecorder.h"
using Microsoft::WRL::ComPtr;
namespace {
void checked(HRESULT hr) { if(FAILED(hr)) throw std::runtime_error("Windows audio endpoint query failed"); }
bool status(const std::filesystem::path& path,const std::string& text) {
    const auto temporary=path.wstring()+L".tmp";
    {std::ofstream out(temporary,std::ios::binary|std::ios::trunc);out<<text;out.flush();if(!out)return false;}
    return MoveFileExW(temporary.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=0;
}
std::wstring endpoint(IMMDeviceEnumerator* enumerator) {
    ComPtr<IMMDevice> device;checked(enumerator->GetDefaultAudioEndpoint(eRender,eMultimedia,&device));
    DWORD state=0;checked(device->GetState(&state));
    if(state!=DEVICE_STATE_ACTIVE)throw std::runtime_error("System output device is not active");
    LPWSTR value=nullptr;checked(device->GetId(&value));
    const std::wstring result=value;CoTaskMemFree(value);return result;
}
}
int WINAPI wWinMain(HINSTANCE,HINSTANCE,PWSTR,int) {
    const auto com=CoInitializeEx(nullptr,COINIT_MULTITHREADED);
    if(FAILED(com))return 2;
    int argc=0;auto raw=CommandLineToArgvW(GetCommandLineW(),&argc);
    std::vector<std::wstring> args;
    if(raw){for(int n=0;n<argc;++n)args.emplace_back(raw[n]);LocalFree(raw);}
    std::filesystem::path output,state;
    HANDLE parent=nullptr;int result=0;
    try {
        if(argc==3 && args[1]==L"--audio-selftest") {
            output=args[2];state=output.wstring()+L".error.txt";
            if(std::filesystem::exists(output))throw std::runtime_error("Refusing to overwrite existing output");
            MiniAudioRecorder::selfTest(output);
        } else {
            if(argc!=8 || args[1]!=L"audio")throw std::runtime_error("This release supports audio recording only");
            output=args[2];state=args[4];const std::filesystem::path stop=args[3];
            if(std::filesystem::exists(output))throw std::runtime_error("Refusing to overwrite existing recording");
            parent=OpenProcess(SYNCHRONIZE,FALSE,static_cast<DWORD>(std::stoul(args[7])));
            if(!parent)throw std::runtime_error("FengYin parent process unavailable");
            ComPtr<IMMDeviceEnumerator> enumerator;
            checked(CoCreateInstance(__uuidof(MMDeviceEnumerator),nullptr,CLSCTX_ALL,IID_PPV_ARGS(&enumerator)));
            const auto id=endpoint(enumerator.Get());
            auto nextCheck=GetTickCount64()+1000;
            MiniAudioRecorder audio;audio.start(output,id);
            if(!status(state,"recording"))throw std::runtime_error("Cannot write recording status");
            while(!std::filesystem::exists(stop) && WaitForSingleObject(parent,0)==WAIT_TIMEOUT) {
                audio.drain();
                if(GetTickCount64()>=nextCheck) {
                    if(endpoint(enumerator.Get())!=id)throw std::runtime_error("System output changed; recording stopped");
                    nextCheck=GetTickCount64()+1000;
                }
                Sleep(10);
            }
            audio.finish();
            if(!status(state,"saved"))throw std::runtime_error("Cannot write completion status");
        }
    } catch(const std::exception& e) {
        if(!state.empty())status(state,std::string("failed: ")+e.what());
        result=1;
    }
    if(parent)CloseHandle(parent);
    CoUninitialize();return result;
}
