#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#include <tlhelp32.h>
#include <objbase.h>
#include <mmdeviceapi.h>
#include <audiopolicy.h>
#include <functiondiscoverykeys_devpkey.h>
#include <propsys.h>
#include <propvarutil.h>
#include <wrl/client.h>
#include "iasiodrv.h"
#include "Tone.h"
#include <atomic>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
#include <algorithm>
#include <cwctype>
#include <cctype>

using Microsoft::WRL::ComPtr;
std::ofstream report;
template<class T> void log(const char* stage, const T& value) { report << stage << ": " << value << std::endl; }
std::string utf8(const wchar_t* value)
{
    const int n = WideCharToMultiByte(CP_UTF8, 0, value, -1, nullptr, 0, nullptr, nullptr);
    if (n <= 0) return {};
    std::string result(n, 0);
    WideCharToMultiByte(CP_UTF8, 0, value, -1, result.data(), n, nullptr, nullptr);
    result.pop_back(); return result;
}
void sessions()
{
    ComPtr<IMMDeviceEnumerator> enumerator;
    if (FAILED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, IID_PPV_ARGS(&enumerator)))) return;
    ComPtr<IMMDeviceCollection> endpoints;
    if (FAILED(enumerator->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &endpoints))) return;
    UINT count = 0; endpoints->GetCount(&count);
    for (UINT i = 0; i < count; ++i)
    {
        ComPtr<IMMDevice> endpoint; if (FAILED(endpoints->Item(i, &endpoint))) continue;
        ComPtr<IPropertyStore> props;
        if (SUCCEEDED(endpoint->OpenPropertyStore(STGM_READ, &props)))
        {
            PROPVARIANT value; PropVariantInit(&value);
            if (SUCCEEDED(props->GetValue(PKEY_Device_FriendlyName, &value)) && value.vt == VT_LPWSTR) log("Endpoint", utf8(value.pwszVal));
            PropVariantClear(&value);
        }
        ComPtr<IAudioSessionManager2> manager;
        if (FAILED(endpoint->Activate(__uuidof(IAudioSessionManager2), CLSCTX_ALL, nullptr, &manager))) continue;
        ComPtr<IAudioSessionEnumerator> list; if (FAILED(manager->GetSessionEnumerator(&list))) continue;
        int n = 0; list->GetCount(&n);
        for (int j = 0; j < n; ++j)
        {
            ComPtr<IAudioSessionControl> control; ComPtr<IAudioSessionControl2> detail;
            if (FAILED(list->GetSession(j, &control)) || FAILED(control.As(&detail))) continue;
            DWORD pid = 0; AudioSessionState state = AudioSessionStateInactive;
            detail->GetProcessId(&pid); control->GetState(&state);
            report << "Session pid=" << pid << " state=" << state;
            if (auto process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid))
            {
                wchar_t name[32768]; DWORD length = 32768;
                if (QueryFullProcessImageNameW(process, 0, name, &length)) report << " exe=" << utf8(std::filesystem::path(name).filename().c_str());
                CloseHandle(process);
            }
            report << std::endl;
        }
    }
}

struct Render
{
    IASIO* driver = nullptr;
    ASIOBufferInfo buffers[2] {};
    int types[2] {};
    long frames = 0;
    bool readySupported = false;
    std::atomic<unsigned long long> rendered { 0 }, callbacks { 0 };
    std::atomic<bool> reset { false };
} render;
void __cdecl bufferSwitch(long index, ASIOBool)
{
    if (index != 0 && index != 1) return;
    const auto start = render.rendered.fetch_add(render.frames);
    for (int channel = 0; channel < 2; ++channel)
    {
        auto* target = static_cast<unsigned char*>(render.buffers[channel].buffers[index]);
        if (! target) continue;
        const auto width = tone::bytes(render.types[channel]);
        for (long frame = 0; frame < render.frames; ++frame)
            tone::encode(target + frame * width, render.types[channel], tone::sample(start + frame));
    }
    render.callbacks.fetch_add(1);
    if (render.readySupported) render.driver->outputReady();
}
void __cdecl rateChanged(ASIOSampleRate) { render.reset.store(true); }
long __cdecl asioMessage(long selector, long value, void*, double*)
{
    if (selector == kAsioSelectorSupported) return value == kAsioResetRequest || value == kAsioEngineVersion;
    if (selector == kAsioEngineVersion) return 2;
    if (selector == kAsioResetRequest) { render.reset.store(true); return 1; }
    return 0;
}
void pump() { MSG message; while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) { TranslateMessage(&message); DispatchMessageW(&message); } }

int probe(int frames)
{
    log("Probe block frames", frames);
    const auto co = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED); log("CoInitialize", co);
    if (FAILED(co)) return 1;
    sessions();
    HKEY root;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\ASIO", 0, KEY_READ | KEY_WOW64_64KEY, &root) != ERROR_SUCCESS) { log("Result", "No 64-bit ASIO registrations"); return 2; }
    CLSID id {}; bool found = false;
    for (DWORD i = 0; ; ++i)
    {
        wchar_t name[256]; DWORD length = 256;
        if (RegEnumKeyExW(root, i, name, &length, nullptr, nullptr, nullptr, nullptr) != ERROR_SUCCESS) break;
        std::wstring lower(name); std::transform(lower.begin(), lower.end(), lower.begin(), towlower);
        if (lower.find(L"asio4all") == std::wstring::npos) continue;
        wchar_t value[128] {}; DWORD bytes = sizeof(value);
        if (RegGetValueW(root, name, L"CLSID", RRF_RT_REG_SZ, nullptr, value, &bytes) == ERROR_SUCCESS && SUCCEEDED(CLSIDFromString(value, &id))) { found = true; log("Driver registration", utf8(name)); break; }
    }
    RegCloseKey(root);
    if (! found) { log("Result", "ASIO4ALL 64-bit not found"); return 2; }
    ComPtr<IASIO> driver;
    auto hr = CoCreateInstance(id, nullptr, CLSCTX_INPROC_SERVER, id, reinterpret_cast<void**>(driver.GetAddressOf()));
    log("CoCreate driver HRESULT", hr); if (FAILED(hr)) return 3;
    auto window = CreateWindowExW(0, L"STATIC", L"FengYin direct ASIO probe", WS_OVERLAPPED, 0, 0, 1, 1, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
    const auto initialised = driver->init(window); log("ASIO init", initialised);
    auto errorText = [&] { char error[124] {}; driver->getErrorMessage(error); error[123] = 0; log("Driver message", error); };
    if (! initialised) { errorText(); return 4; }
    if (frames == 0)
    {
        MessageBoxW(nullptr,L"下一步打开本工具的 ASIO4ALL 面板。\n\n点击右下角齿轮进入高级设置，展开 Realtek：\n只启用实际使用的扬声器/耳机输出；关闭麦克风输入和风吟共享扬声器。\n\n完成后关闭面板，再在确认窗口点击确定。",L"先选择实体输出",MB_OK|MB_ICONINFORMATION);
        const auto panel = driver->controlPanel(); log("controlPanel", panel);
        if (panel != ASE_OK) { errorText(); return 11; }
        const auto confirmed = MessageBoxW(nullptr,L"请确认已在 ASIO4ALL 中启用 Realtek 实际输出，并关闭配置面板。\n\n点击确定后，将释放驱动，再由新进程重新连接并测试。\n尚未配置好可点击取消。",L"配置完成后继续",MB_OKCANCEL|MB_ICONINFORMATION);
        driver.Reset(); DestroyWindow(window); CoUninitialize();
        return confirmed == IDOK ? 0 : 12;
    }
    char name[32] {}; driver->getDriverName(name); name[31] = 0; log("Driver name", name); log("Driver version", driver->getDriverVersion());
    auto result = driver->canSampleRate(48000); log("canSampleRate 48000", result); if (result != ASE_OK) { errorText(); return 5; }
    result = driver->setSampleRate(48000); log("setSampleRate 48000", result); if (result != ASE_OK) { errorText(); return 5; }
    double actualRate = 0; result = driver->getSampleRate(&actualRate); log("actualRate", actualRate);
    if (result != ASE_OK || actualRate != 48000) return 5;
    long inputs = 0, outputs = 0;
    result = driver->getChannels(&inputs, &outputs); log("getChannels", result); log("inputs (NOT opened)", inputs); log("outputs", outputs);
    if (result != ASE_OK || outputs < 2 || outputs > 256) return 6;
    long min = 0, max = 0, preferred = 0, granularity = 0;
    result = driver->getBufferSize(&min, &max, &preferred, &granularity);
    report << "Buffer capabilities result=" << result << " min=" << min << " max=" << max << " preferred=" << preferred << " granularity=" << granularity << std::endl;
    if (result != ASE_OK || frames < min || frames > max || (granularity > 0 && (frames-min)%granularity != 0)
        || (granularity == 0 && frames != preferred) || (granularity == -1 && (frames & (frames-1)) != 0)) return 7;
    std::vector<ASIOChannelInfo> usable;
    for (long i = 0; i < outputs; ++i)
    {
        ASIOChannelInfo info {}; info.channel = i; info.isInput = ASIOFalse;
        result = driver->getChannelInfo(&info); info.name[31] = 0;
        report << "Output " << i << " result=" << result << " name=" << info.name << " group=" << info.channelGroup << " type=" << info.type << std::endl;
        std::string lower(info.name); std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c){return static_cast<char>(std::tolower(c));});
        if (result == ASE_OK && !lower.empty() && lower.find("not connected") == std::string::npos && lower.find("fengyin") == std::string::npos
            && lower.find("风吟") == std::string::npos && lower.find("virtual") == std::string::npos && tone::bytes(info.type)) usable.push_back(info);
    }
    int first = -1;
    for (int i = 0; i + 1 < static_cast<int>(usable.size()); ++i)
        if (usable[i].channelGroup == usable[i+1].channelGroup && usable[i+1].channel == usable[i].channel+1) { first = i; break; }
    if (first < 0) { log("Result", "No connected physical stereo pair; no tone attempted"); errorText(); return 8; }
    render.driver = driver.Get(); render.frames = frames;
    for (int i = 0; i < 2; ++i) { render.buffers[i].isInput = ASIOFalse; render.buffers[i].channelNum = usable[first+i].channel; render.types[i] = usable[first+i].type; log("Selected output", usable[first+i].name); }
    ASIOCallbacks callbacks {}; callbacks.bufferSwitch = bufferSwitch; callbacks.sampleRateDidChange = rateChanged; callbacks.asioMessage = asioMessage;
    result = driver->createBuffers(render.buffers, 2, frames, &callbacks); log("createBuffers outputs-only", result);
    if (result != ASE_OK) { errorText(); return 9; }
    for (int c = 0; c < 2; ++c) for (int b = 0; b < 2; ++b)
    {
        if (!render.buffers[c].buffers[b]) { log("Result", "Null output buffer"); driver->disposeBuffers(); return 9; }
        std::memset(render.buffers[c].buffers[b], 0, frames*tone::bytes(render.types[c]));
    }
    long inLatency = 0, outLatency = 0; log("getLatencies", driver->getLatencies(&inLatency, &outLatency)); log("outputLatency", outLatency);
    render.readySupported = driver->outputReady() == ASE_OK;
    render.reset.store(false); result = driver->start(); log("start", result);
    if (result == ASE_OK)
    {
        const auto until = GetTickCount64()+3000;
        while (GetTickCount64() < until && !render.reset.load()) { pump(); Sleep(5); }
        log("stop", driver->stop());
    }
    errorText(); log("callbacks", render.callbacks.load()); log("rendered frames", render.rendered.load()); log("reset requested", render.reset.load());
    log("disposeBuffers", driver->disposeBuffers());
    driver.Reset(); DestroyWindow(window); sessions(); CoUninitialize();
    log("Result", "Audibility must be confirmed by listener; callbacks are NOT proof of sound");
    return result == ASE_OK && render.callbacks.load() > 0 && !render.reset.load() ? 0 : 10;
}

bool fengyinRunning()
{
    auto snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) return true;
    PROCESSENTRY32W entry {}; entry.dwSize = sizeof(entry); bool found = false;
    if (Process32FirstW(snapshot, &entry)) do
    { if (!_wcsicmp(entry.szExeFile,L"FengYin.exe") || !_wcsicmp(entry.szExeFile,L"FengYinAudioEngine.exe")) { found=true; break; } } while(Process32NextW(snapshot,&entry));
    CloseHandle(snapshot); return found;
}
int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int)
{
    int argc = 0; auto argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (!argv) return 1;
    if (argc == 4 && std::wstring(argv[1]) == L"--probe")
    {
        report.open(std::filesystem::path(argv[3]), std::ios::app | std::ios::binary);
        const int frames = _wtoi(argv[2]); LocalFree(argv);
        if (!report || (frames != 0 && frames != 128 && frames != 256)) return 1;
        return probe(frames);
    }
    LocalFree(argv);
    if (MessageBoxW(nullptr,L"请先关闭风吟、浏览器、微信和其他播放器，并调低耳机/音箱音量。\n\n将直接测试ASIO4ALL：先128帧、再256帧，每次约2秒轻音。\n不安装驱动、不修改系统输出设备。报告包含设备名和音频进程名，不记录音频。\n\n点击“确定”开始。",L"风吟独立声音自检",MB_OKCANCEL|MB_ICONINFORMATION)!=IDOK) return 0;
    if (fengyinRunning()) { MessageBoxW(nullptr,L"请完全退出风吟后再运行本工具。不会强制关闭你的程序。",L"请先退出风吟",MB_OK); return 1; }
    PWSTR desktop = nullptr;
    if (FAILED(SHGetKnownFolderPath(FOLDERID_Desktop, 0, nullptr, &desktop))) return 1;
    SYSTEMTIME time; GetLocalTime(&time); wchar_t filename[128];
    swprintf_s(filename,L"风吟独立ASIO自检-%04u%02u%02u-%02u%02u%02u-%lu.txt",time.wYear,time.wMonth,time.wDay,time.wHour,time.wMinute,time.wSecond,GetCurrentProcessId());
    auto path = std::filesystem::path(desktop)/filename; CoTaskMemFree(desktop);
    { std::ofstream output(path,std::ios::binary); output << "\xEF\xBB\xBF" << "FengYin direct ASIO self-test v2; configure then fresh-process probes; no JUCE, no inputs\nASIO4ALL settings can differ between host executables.\n"; if(!output) { MessageBoxW(nullptr,L"无法写入桌面报告。",L"无法开始",MB_OK); return 1; } }
    wchar_t executable[32768]; GetModuleFileNameW(nullptr,executable,32768);
    for (int frames : {0,128,256})
    {
        auto command = L"\""+std::wstring(executable)+L"\" --probe "+std::to_wstring(frames)+L" \""+path.wstring()+L"\"";
        STARTUPINFOW startup {}; startup.cb=sizeof(startup); PROCESS_INFORMATION process {};
        if (!CreateProcessW(nullptr,command.data(),nullptr,nullptr,FALSE,0,nullptr,nullptr,&startup,&process)) { std::ofstream(path,std::ios::app)<<"CreateProcess failed "<<GetLastError()<<std::endl; break; }
        CloseHandle(process.hThread);
        const auto until = GetTickCount64()+(frames == 0 ? 600000 : 15000);
        while (WaitForSingleObject(process.hProcess,0)==WAIT_TIMEOUT && GetTickCount64()<until) { pump(); Sleep(20); }
        if (WaitForSingleObject(process.hProcess,0)==WAIT_TIMEOUT) { TerminateProcess(process.hProcess,124); WaitForSingleObject(process.hProcess,2000); }
        DWORD code=0; GetExitCodeProcess(process.hProcess,&code); CloseHandle(process.hProcess);
        std::ofstream(path,std::ios::app)<<"Probe exit code="<<code<<std::endl;
        if (frames == 0)
        {
            if (code != 0) { MessageBoxW(nullptr,L"配置未完成，已停止声音测试。请把生成的报告发回。",L"配置未完成",MB_OK); break; }
            continue;
        }
        const auto prompt=std::to_wstring(frames)+L"帧测试结束。\n\n你听到刚才的短音了吗？\n“是”=听到，“否”=没听到，“取消”=停止后续测试。";
        const auto heard=MessageBoxW(nullptr,prompt.c_str(),L"记录测试结果",MB_YESNOCANCEL|MB_ICONQUESTION);
        std::ofstream(path,std::ios::app)<<"Listener frames="<<frames<<" answer="<<(heard==IDYES?"heard":heard==IDNO?"not heard":"cancelled")<<std::endl;
        if(heard==IDCANCEL || code==124 || code>=0x80000000u) break;
    }
    MessageBoxW(nullptr,L"自检结束。桌面已生成“风吟独立ASIO自检”报告，请把它发回。\n测试结果不代表问题已修复。",L"报告已生成",MB_OK);
    ShellExecuteW(nullptr,L"open",path.c_str(),nullptr,nullptr,SW_SHOWNORMAL);
    return 0;
}
