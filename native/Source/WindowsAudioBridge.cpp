#include "WindowsAudioBridge.h"
#include <cmath>

#if JUCE_WINDOWS
 #include <windows.h>
 #include <shellapi.h>
 #include <mmdeviceapi.h>
 #include <functiondiscoverykeys_devpkey.h>
 #include <propvarutil.h>
#endif

namespace
{
juce::String utf8(const char* text) { return juce::String::fromUTF8(text); }

#if JUCE_WINDOWS
juce::String readRegistryString(HKEY root, const wchar_t* path, const wchar_t* name)
{
    wchar_t value[2048] {};
    DWORD type = 0, bytes = sizeof(value);
    if (RegGetValueW(root, path, name, RRF_RT_REG_SZ | RRF_RT_REG_EXPAND_SZ,
                     &type, value, &bytes) != ERROR_SUCCESS)
        return {};
    return juce::String(value);
}

class ScopedCom final
{
public:
    ScopedCom() : result(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED)) {}
    ~ScopedCom() { if (result == S_OK || result == S_FALSE) CoUninitialize(); }
    [[nodiscard]] bool available() const noexcept { return SUCCEEDED(result) || result == RPC_E_CHANGED_MODE; }
private:
    HRESULT result;
};

struct __declspec(uuid("568b9108-44bf-40b4-9006-86afe5b5a620")) PolicyConfigVista : IUnknown
{
    virtual HRESULT STDMETHODCALLTYPE GetMixFormat(PCWSTR, WAVEFORMATEX**) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetDeviceFormat(PCWSTR, INT, WAVEFORMATEX**) = 0;
    virtual HRESULT STDMETHODCALLTYPE ResetDeviceFormat(PCWSTR) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetDeviceFormat(PCWSTR, WAVEFORMATEX*, WAVEFORMATEX*) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetProcessingPeriod(PCWSTR, INT, PINT64, PINT64) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetProcessingPeriod(PCWSTR, PINT64) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetShareMode(PCWSTR, void*) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetShareMode(PCWSTR, void*) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetPropertyValue(PCWSTR, const PROPERTYKEY&, PROPVARIANT*) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetPropertyValue(PCWSTR, const PROPERTYKEY&, PROPVARIANT*) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetDefaultEndpoint(PCWSTR, ERole) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetEndpointVisibility(PCWSTR, INT) = 0;
};

const CLSID policyConfigClient = { 0x870af99c, 0x171d, 0x4f9e, { 0xaf, 0x0d, 0xe6, 0x3d, 0xf4, 0x0c, 0x2b, 0xc9 } };

juce::String defaultRenderEndpointId()
{
    ScopedCom com;
    if (! com.available()) return {};
    juce::String result;
    IMMDeviceEnumerator* enumerator = nullptr;
    IMMDevice* device = nullptr;
    LPWSTR id = nullptr;
    if (SUCCEEDED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                                   __uuidof(IMMDeviceEnumerator), reinterpret_cast<void**>(&enumerator)))
        && SUCCEEDED(enumerator->GetDefaultAudioEndpoint(eRender, eMultimedia, &device))
        && SUCCEEDED(device->GetId(&id)))
        result = juce::String(id);
    if (id != nullptr) CoTaskMemFree(id);
    if (device != nullptr) device->Release();
    if (enumerator != nullptr) enumerator->Release();
    return result;
}

juce::String findRenderEndpointId(const juce::String& namePart)
{
    ScopedCom com;
    if (! com.available()) return {};
    juce::String result;
    IMMDeviceEnumerator* enumerator = nullptr;
    IMMDeviceCollection* collection = nullptr;
    if (FAILED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                                __uuidof(IMMDeviceEnumerator), reinterpret_cast<void**>(&enumerator))))
        return {};
    if (SUCCEEDED(enumerator->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &collection)))
    {
        UINT count = 0;
        collection->GetCount(&count);
        for (UINT i = 0; i < count && result.isEmpty(); ++i)
        {
            IMMDevice* device = nullptr;
            IPropertyStore* store = nullptr;
            PROPVARIANT value;
            PropVariantInit(&value);
            LPWSTR id = nullptr;
            if (SUCCEEDED(collection->Item(i, &device))
                && SUCCEEDED(device->OpenPropertyStore(STGM_READ, &store))
                && SUCCEEDED(store->GetValue(PKEY_Device_FriendlyName, &value)))
            {
                const juce::String name(value.pwszVal != nullptr ? value.pwszVal : L"");
                if (name.containsIgnoreCase(namePart) && SUCCEEDED(device->GetId(&id)))
                    result = juce::String(id);
            }
            if (id != nullptr) CoTaskMemFree(id);
            PropVariantClear(&value);
            if (store != nullptr) store->Release();
            if (device != nullptr) device->Release();
        }
    }
    if (collection != nullptr) collection->Release();
    enumerator->Release();
    return result;
}

bool setDefaultRenderEndpoint(const juce::String& id)
{
    if (id.isEmpty()) return false;
    ScopedCom com;
    if (! com.available()) return false;
    PolicyConfigVista* policy = nullptr;
    if (FAILED(CoCreateInstance(policyConfigClient, nullptr, CLSCTX_ALL,
                                __uuidof(PolicyConfigVista), reinterpret_cast<void**>(&policy))))
        return false;
    const auto* endpoint = id.toWideCharPointer();
    const bool ok = SUCCEEDED(policy->SetDefaultEndpoint(endpoint, eConsole))
                 && SUCCEEDED(policy->SetDefaultEndpoint(endpoint, eMultimedia))
                 && SUCCEEDED(policy->SetDefaultEndpoint(endpoint, eCommunications));
    policy->Release();
    return ok;
}
#endif
}

namespace fengyin
{
juce::File WindowsAudioBridge::stateFile() const
{
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("FengYin").getChildFile("audio-bridge-state.json");
}

juce::File WindowsAudioBridge::installerScript() const
{
    return juce::File::getSpecialLocation(juce::File::currentExecutableFile)
        .getParentDirectory().getChildFile("tools").getChildFile("audio-bridge").getChildFile("install.ps1");
}

juce::File WindowsAudioBridge::voiceMeeterRemoteDll() const
{
#if JUCE_WINDOWS
    auto folder = readRegistryString(HKEY_LOCAL_MACHINE, L"SOFTWARE\\VB-Audio\\Voicemeeter", L"UninstallDir");
    if (folder.isEmpty())
        folder = readRegistryString(HKEY_LOCAL_MACHINE, L"SOFTWARE\\WOW6432Node\\VB-Audio\\Voicemeeter", L"UninstallDir");
    if (folder.isNotEmpty())
    {
        const auto dll = juce::File(folder).getChildFile("VoicemeeterRemote64.dll");
        if (dll.existsAsFile()) return dll;
    }
    const auto programFiles = juce::File::getSpecialLocation(juce::File::globalApplicationsDirectory);
    for (const auto& relative : { "VB/Voicemeeter/VoicemeeterRemote64.dll", "VB-Audio/Voicemeeter/VoicemeeterRemote64.dll" })
    {
        const auto dll = programFiles.getChildFile(relative);
        if (dll.existsAsFile()) return dll;
    }
#endif
    return {};
}

juce::String WindowsAudioBridge::findVoiceMeeterAsioName() const
{
#if JUCE_WINDOWS
    for (const auto& key : { L"SOFTWARE\\ASIO\\Voicemeeter Virtual ASIO",
                             L"SOFTWARE\\WOW6432Node\\ASIO\\Voicemeeter Virtual ASIO" })
    {
        if (readRegistryString(HKEY_LOCAL_MACHINE, key, L"CLSID").isNotEmpty())
            return "Voicemeeter Virtual ASIO";
    }
#endif
    return {};
}

AudioBridgeStatus WindowsAudioBridge::getStatus() const
{
    AudioBridgeStatus status;
#if JUCE_WINDOWS
    status.supported = true;
    const bool vm = voiceMeeterRemoteDll().existsAsFile() && findVoiceMeeterAsioName().isNotEmpty();
    const bool asio4all = readRegistryString(HKEY_LOCAL_MACHINE, L"SOFTWARE\\ASIO\\ASIO4ALL v2", L"CLSID").isNotEmpty()
                       || readRegistryString(HKEY_LOCAL_MACHINE, L"SOFTWARE\\WOW6432Node\\ASIO\\ASIO4ALL v2", L"CLSID").isNotEmpty();
    status.installed = vm && asio4all;
    const auto json = juce::JSON::parse(stateFile().loadFileAsString());
    if (json.isObject())
    {
        status.state = json.getProperty("state", "idle").toString();
        status.message = json.getProperty("message", {}).toString();
        status.running = status.state == "running";
        status.restartRequired = status.state == "restart";
        status.active = status.state == "active";
        status.canRestore = json.getProperty("originalEndpointId", {}).toString().isNotEmpty();
        if (status.restartRequired && status.installed)
        {
            const auto bootTimeMs = juce::Time::getCurrentTime().toMilliseconds()
                                  - static_cast<juce::int64>(GetTickCount64());
            if (stateFile().getLastModificationTime().toMilliseconds() < bootTimeMs)
            {
                status.restartRequired = false;
                status.state = "ready";
                status.message = utf8("组件已安装并完成重启，点击启用桥接");
            }
        }
    }
    if (status.message.isEmpty())
        status.message = status.installed ? utf8("组件已安装，可启用 48 kHz / 128 samples 桥接")
                                          : utf8("需要安装 VoiceMeeter Banana 与 ASIO4ALL（不会替换声卡驱动）");
#else
    status.message = utf8("共享低延迟桥接仅支持 Windows");
#endif
    return status;
}

bool WindowsAudioBridge::launchInstaller()
{
#if JUCE_WINDOWS
    const auto script = installerScript();
    if (! script.existsAsFile()) return false;
    stateFile().getParentDirectory().createDirectory();
    auto state = std::make_unique<juce::DynamicObject>();
    state->setProperty("state", "running");
    state->setProperty("message", utf8("正在下载安装官方桥接组件…"));
    stateFile().replaceWithText(juce::JSON::toString(juce::var(state.release())));
    const auto parameters = juce::String("-NoProfile -ExecutionPolicy Bypass -File \"")
        + script.getFullPathName() + "\" -ResultFile \"" + stateFile().getFullPathName() + "\"";
    const auto result = reinterpret_cast<INT_PTR>(ShellExecuteW(nullptr, L"runas", L"powershell.exe",
        parameters.toWideCharPointer(), nullptr, SW_SHOWNORMAL));
    return result > 32;
#else
    return false;
#endif
}

juce::String WindowsAudioBridge::configureVoiceMeeter()
{
#if JUCE_WINDOWS
    const auto dllFile = voiceMeeterRemoteDll();
    if (! dllFile.existsAsFile()) return utf8("未找到 VoiceMeeter Remote 组件，请先安装并重启电脑");
    HMODULE module = LoadLibraryW(dllFile.getFullPathName().toWideCharPointer());
    if (module == nullptr) return utf8("无法载入 VoiceMeeter Remote 组件");
    using Login = long (__stdcall*)();
    using Logout = long (__stdcall*)();
    using Run = long (__stdcall*)(long);
    using SetFloat = long (__stdcall*)(char*, float);
    using SetString = long (__stdcall*)(char*, char*);
    using GetString = long (__stdcall*)(char*, char*);
    using GetFloat = long (__stdcall*)(char*, float*);
    const auto login = reinterpret_cast<Login>(GetProcAddress(module, "VBVMR_Login"));
    const auto logout = reinterpret_cast<Logout>(GetProcAddress(module, "VBVMR_Logout"));
    const auto run = reinterpret_cast<Run>(GetProcAddress(module, "VBVMR_RunVoicemeeter"));
    const auto setFloat = reinterpret_cast<SetFloat>(GetProcAddress(module, "VBVMR_SetParameterFloat"));
    const auto setString = reinterpret_cast<SetString>(GetProcAddress(module, "VBVMR_SetParameterStringA"));
    const auto getString = reinterpret_cast<GetString>(GetProcAddress(module, "VBVMR_GetParameterStringA"));
    const auto getFloat = reinterpret_cast<GetFloat>(GetProcAddress(module, "VBVMR_GetParameterFloat"));
    if (login == nullptr || logout == nullptr || run == nullptr || setFloat == nullptr || setString == nullptr
        || getString == nullptr || getFloat == nullptr)
    {
        FreeLibrary(module);
        return utf8("VoiceMeeter Remote API 不完整");
    }
    const auto logged = login();
    if (logged < 0) { FreeLibrary(module); return utf8("无法连接 VoiceMeeter"); }
    if (logged == 1) { run(2); juce::Thread::sleep(1800); }
    char bus[] = "Bus[0].Device.asio", asio[] = "ASIO4ALL v2";
    char sr[] = "Option.sr", asioSr[] = "Option.ASIOsr", buffer[] = "Option.buffer.asio";
    char strip3[] = "Strip[3].A1", strip4[] = "Strip[4].A1", restart[] = "Command.Restart";
    bool ok = setString(bus, asio) >= 0
                 && setFloat(sr, 48000.0f) >= 0
                 && setFloat(asioSr, 1.0f) >= 0
                 && setFloat(buffer, 128.0f) >= 0
                 && setFloat(strip3, 1.0f) >= 0
                 && setFloat(strip4, 1.0f) >= 0
                 && setFloat(restart, 1.0f) >= 0;
    if (ok)
    {
        juce::Thread::sleep(1800);
        char deviceParam[] = "Bus[0].device.name", rateParam[] = "Bus[0].device.sr";
        char deviceName[512] {};
        float actualRate = 0.0f;
        ok = getString(deviceParam, deviceName) >= 0
          && getFloat(rateParam, &actualRate) >= 0
          && juce::String(deviceName).containsIgnoreCase("ASIO4ALL")
          && std::abs(actualRate - 48000.0f) < 1.0f;
    }
    logout();
    FreeLibrary(module);
    return ok ? juce::String() : utf8("VoiceMeeter 无法应用 ASIO4ALL / 128 samples 配置");
#else
    return utf8("仅支持 Windows");
#endif
}

juce::String WindowsAudioBridge::routeWindowsAudioToVoiceMeeter()
{
#if JUCE_WINDOWS
    const auto original = defaultRenderEndpointId();
    const auto voiceMeeter = findRenderEndpointId("VoiceMeeter Input");
    if (voiceMeeter.isEmpty()) return utf8("未找到 VoiceMeeter Input 系统播放设备，请先重启电脑");
    if (! setDefaultRenderEndpoint(voiceMeeter)) return utf8("无法把 Windows 系统声音切换到 VoiceMeeter Input");
    auto object = std::make_unique<juce::DynamicObject>();
    object->setProperty("state", "active");
    object->setProperty("message", utf8("桥接已启用：系统声音与风吟共同输出，目标 48 kHz / 128 samples"));
    object->setProperty("originalEndpointId", original);
    stateFile().getParentDirectory().createDirectory();
    stateFile().replaceWithText(juce::JSON::toString(juce::var(object.release())));
    return {};
#else
    return utf8("仅支持 Windows");
#endif
}

juce::String WindowsAudioBridge::restoreWindowsAudio()
{
#if JUCE_WINDOWS
    auto json = juce::JSON::parse(stateFile().loadFileAsString());
    const auto original = json.getProperty("originalEndpointId", {}).toString();
    if (original.isNotEmpty() && ! setDefaultRenderEndpoint(original))
        return utf8("无法恢复原来的 Windows 输出设备，请在系统声音设置中手动选择");
    stateFile().deleteFile();
    return {};
#else
    return utf8("仅支持 Windows");
#endif
}
}
