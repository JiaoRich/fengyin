#if defined(_WIN32)
#include "DefaultEndpointRouter.h"
#include "NamedSharedAudioRegion.h"

#include <windows.h>
#include <newdev.h>
#include <setupapi.h>
#include <shellapi.h>
#include <cfgmgr32.h>
#include <mmdeviceapi.h>
#include <functiondiscoverykeys_devpkey.h>
#include <propvarutil.h>

#include <chrono>
#include <cwctype>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>
#include <vector>

namespace
{
constexpr wchar_t hardwareId[] = L"Root\\FengYinAudioEngine";
void log(const std::wstring& message);

bool stopAudioEngine()
{
    const auto singleton = OpenMutexW(SYNCHRONIZE | MUTEX_MODIFY_STATE, FALSE,
                                      fengyin::audioengine::engineSingletonName);
    if (singleton == nullptr) return true;
    if (const auto stopEvent = OpenEventW(EVENT_MODIFY_STATE, FALSE,
                                          fengyin::audioengine::engineStopEventName))
    {
        SetEvent(stopEvent);
        CloseHandle(stopEvent);
    }
    const auto stopped = WaitForSingleObject(singleton, 5000);
    if (stopped == WAIT_OBJECT_0 || stopped == WAIT_ABANDONED)
        ReleaseMutex(singleton);
    CloseHandle(singleton);
    return stopped == WAIT_OBJECT_0 || stopped == WAIT_ABANDONED;
}

bool prepareForDriverChange()
{
    if (! stopAudioEngine())
    {
        log(L"Audio engine did not stop before driver change");
        return false;
    }
    std::wstring error;
    if (! fengyin::audioengine::DefaultEndpointRouter::restorePendingRoute(error))
    {
        log(L"Default endpoint recovery failed before driver change: " + error);
        return false;
    }
    return true;
}

std::filesystem::path logPath()
{
    wchar_t programData[MAX_PATH] {};
    const auto length = GetEnvironmentVariableW(L"ProgramData", programData, MAX_PATH);
    auto folder = length > 0 && length < MAX_PATH
        ? std::filesystem::path(programData) / L"FengYin"
        : std::filesystem::temp_directory_path() / L"FengYin";
    std::error_code ignored;
    std::filesystem::create_directories(folder, ignored);
    return folder / L"driver-setup.log";
}

std::filesystem::path installedInfRecordPath()
{
    auto path = logPath();
    path.replace_filename(L"audio-driver-inf.txt");
    return path;
}

void rememberInstalledInf(const std::filesystem::path& path)
{
    std::wofstream stream(installedInfRecordPath(), std::ios::trunc);
    stream << path.filename().wstring();
}

std::wstring readInstalledInf()
{
    std::wifstream stream(installedInfRecordPath());
    std::wstring name;
    std::getline(stream, name);
    return std::filesystem::path(name).filename().wstring();
}

void log(const std::wstring& message)
{
    std::wofstream stream(logPath(), std::ios::app);
    SYSTEMTIME now {};
    GetLocalTime(&now);
    stream << now.wYear << L'-' << now.wMonth << L'-' << now.wDay << L' '
           << now.wHour << L':' << now.wMinute << L':' << now.wSecond
           << L"  " << message << L'\n';
}

bool equalsIgnoreCase(const wchar_t* first, const wchar_t* second)
{
    return first != nullptr && second != nullptr && _wcsicmp(first, second) == 0;
}

bool deviceHasHardwareId(HDEVINFO devices, SP_DEVINFO_DATA& device)
{
    DWORD required = 0;
    SetupDiGetDeviceRegistryPropertyW(devices, &device, SPDRP_HARDWAREID, nullptr,
                                      nullptr, 0, &required);
    if (required == 0 || GetLastError() != ERROR_INSUFFICIENT_BUFFER) return false;
    std::vector<BYTE> bytes(required + sizeof(wchar_t), 0);
    if (! SetupDiGetDeviceRegistryPropertyW(devices, &device, SPDRP_HARDWAREID, nullptr,
                                            bytes.data(), required, nullptr))
        return false;
    for (auto* value = reinterpret_cast<const wchar_t*>(bytes.data()); *value != L'\0';
         value += std::wcslen(value) + 1)
        if (equalsIgnoreCase(value, hardwareId)) return true;
    return false;
}

bool deviceExists()
{
    const auto devices = SetupDiGetClassDevsW(nullptr, nullptr, nullptr,
                                               DIGCF_ALLCLASSES | DIGCF_PRESENT);
    if (devices == INVALID_HANDLE_VALUE) return false;
    bool found = false;
    SP_DEVINFO_DATA device { sizeof(device) };
    for (DWORD index = 0; SetupDiEnumDeviceInfo(devices, index, &device); ++index)
        if (deviceHasHardwareId(devices, device)) { found = true; break; }
    SetupDiDestroyDeviceInfoList(devices);
    return found;
}

struct DriverDeviceState
{
    bool exists = false;
    bool started = false;
    ULONG status = 0;
    ULONG problem = 0;
};

DriverDeviceState queryDriverDeviceState()
{
    DriverDeviceState result;
    const auto devices = SetupDiGetClassDevsW(nullptr, nullptr, nullptr,
                                               DIGCF_ALLCLASSES | DIGCF_PRESENT);
    if (devices == INVALID_HANDLE_VALUE) return result;
    SP_DEVINFO_DATA device { sizeof(device) };
    for (DWORD index = 0; SetupDiEnumDeviceInfo(devices, index, &device); ++index)
    {
        if (! deviceHasHardwareId(devices, device)) continue;
        result.exists = true;
        if (CM_Get_DevNode_Status(&result.status, &result.problem, device.DevInst, 0) == CR_SUCCESS)
            result.started = (result.status & DN_STARTED) != 0 && result.problem == CM_PROB_NONE;
        break;
    }
    SetupDiDestroyDeviceInfoList(devices);
    return result;
}

bool containsIgnoreCase(std::wstring value, std::wstring needle)
{
    for (auto& character : value) character = static_cast<wchar_t>(towlower(character));
    for (auto& character : needle) character = static_cast<wchar_t>(towlower(character));
    return value.find(needle) != std::wstring::npos;
}

bool activeRenderEndpointExists()
{
    IMMDeviceEnumerator* enumerator = nullptr;
    IMMDeviceCollection* endpoints = nullptr;
    auto found = false;
    if (FAILED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                                __uuidof(IMMDeviceEnumerator), reinterpret_cast<void**>(&enumerator))))
        return false;
    if (SUCCEEDED(enumerator->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &endpoints)))
    {
        UINT count = 0;
        endpoints->GetCount(&count);
        for (UINT index = 0; index < count && ! found; ++index)
        {
            IMMDevice* endpoint = nullptr;
            IPropertyStore* properties = nullptr;
            LPWSTR id = nullptr;
            PROPVARIANT name;
            PropVariantInit(&name);
            if (SUCCEEDED(endpoints->Item(index, &endpoint)) && endpoint != nullptr)
            {
                endpoint->GetId(&id);
                if (SUCCEEDED(endpoint->OpenPropertyStore(STGM_READ, &properties)) && properties != nullptr)
                    properties->GetValue(PKEY_Device_FriendlyName, &name);
                const std::wstring endpointId = id != nullptr ? id : L"";
                const std::wstring friendlyName = name.vt == VT_LPWSTR && name.pwszVal != nullptr
                    ? name.pwszVal : L"";
                found = containsIgnoreCase(endpointId, L"fengyin")
                     || containsIgnoreCase(friendlyName, L"fengyin")
                     || containsIgnoreCase(friendlyName, L"风吟共享扬声器");
            }
            PropVariantClear(&name);
            if (id != nullptr) CoTaskMemFree(id);
            if (properties != nullptr) properties->Release();
            if (endpoint != nullptr) endpoint->Release();
        }
    }
    if (endpoints != nullptr) endpoints->Release();
    enumerator->Release();
    return found;
}

bool installationIsOperational(bool writeDiagnostic)
{
    const auto state = queryDriverDeviceState();
    const auto endpointReady = state.started && activeRenderEndpointExists();
    if (writeDiagnostic)
        log(L"Driver verification: exists=" + std::to_wstring(state.exists)
            + L", started=" + std::to_wstring(state.started)
            + L", status=" + std::to_wstring(state.status)
            + L", problem=" + std::to_wstring(state.problem)
            + L", endpoint=" + std::to_wstring(endpointReady));
    return state.exists && state.started && endpointReady;
}

bool waitForOperationalInstallation()
{
    for (int attempt = 0; attempt < 40; ++attempt)
    {
        if (installationIsOperational(false)) return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
    (void) installationIsOperational(true);
    return false;
}

bool createRootDevice(const std::filesystem::path& infPath)
{
    GUID classGuid {};
    constexpr DWORD classNameCapacity = 256;
    wchar_t className[classNameCapacity] {};
    if (! SetupDiGetINFClassW(infPath.c_str(), &classGuid, className, classNameCapacity, nullptr))
    {
        log(L"SetupDiGetINFClass failed: " + std::to_wstring(GetLastError()));
        return false;
    }
    const auto devices = SetupDiCreateDeviceInfoList(&classGuid, nullptr);
    if (devices == INVALID_HANDLE_VALUE) return false;
    SP_DEVINFO_DATA device { sizeof(device) };
    auto ok = SetupDiCreateDeviceInfoW(devices, className, &classGuid,
                                       L"FengYin Shared Speaker", nullptr,
                                       DICD_GENERATE_ID, &device) != FALSE;
    const std::wstring ids = std::wstring(hardwareId) + L'\0' + L'\0';
    if (ok)
        ok = SetupDiSetDeviceRegistryPropertyW(devices, &device, SPDRP_HARDWAREID,
                                               reinterpret_cast<const BYTE*>(ids.data()),
                                               static_cast<DWORD>(ids.size() * sizeof(wchar_t))) != FALSE;
    if (ok) ok = SetupDiCallClassInstaller(DIF_REGISTERDEVICE, devices, &device) != FALSE;
    if (! ok) log(L"Create root device failed: " + std::to_wstring(GetLastError()));
    SetupDiDestroyDeviceInfoList(devices);
    return ok;
}

int uninstall();

int install(const std::filesystem::path& suppliedInf)
{
    if (! prepareForDriverChange()) return 9;
    std::error_code ignored;
    const auto infPath = std::filesystem::absolute(suppliedInf, ignored);
    if (ignored || ! std::filesystem::is_regular_file(infPath))
    {
        log(L"INF not found: " + suppliedInf.wstring());
        return 2;
    }
    const auto existedBeforeInstall = deviceExists();
    wchar_t copiedInf[MAX_PATH] {};
    bool copiedByTransaction = false;
    if (! SetupCopyOEMInfW(infPath.c_str(), nullptr, SPOST_PATH, 0, copiedInf, MAX_PATH,
                           nullptr, nullptr))
    {
        const auto error = GetLastError();
        if (error != ERROR_FILE_EXISTS)
        {
            log(L"SetupCopyOEMInf failed: " + std::to_wstring(error));
            return 3;
        }
    }
    else
    {
        copiedByTransaction = ! existedBeforeInstall;
    }
    if (! existedBeforeInstall && ! createRootDevice(infPath))
    {
        if (copiedByTransaction && copiedInf[0] != L'\0')
            (void) SetupUninstallOEMInfW(std::filesystem::path(copiedInf).filename().c_str(), 0, nullptr);
        return 4;
    }
    BOOL reboot = FALSE;
    if (! UpdateDriverForPlugAndPlayDevicesW(nullptr, hardwareId, infPath.c_str(),
                                              INSTALLFLAG_FORCE, &reboot))
    {
        log(L"UpdateDriverForPlugAndPlayDevices failed: " + std::to_wstring(GetLastError()));
        // Roll back only the root device created by this transaction. Never
        // remove a previously working installation during an upgrade failure.
        if (! existedBeforeInstall) (void) uninstall();
        if (copiedByTransaction && copiedInf[0] != L'\0')
            (void) SetupUninstallOEMInfW(std::filesystem::path(copiedInf).filename().c_str(), 0, nullptr);
        return 5;
    }
    if (copiedInf[0] != L'\0') rememberInstalledInf(copiedInf);
    if (! reboot && ! waitForOperationalInstallation())
    {
        log(L"Driver package installed but the device did not start or publish an active render endpoint");
        return 10;
    }
    log(reboot ? L"Driver installed; reboot required" : L"Driver installed");
    return reboot ? 3010 : 0;
}

int uninstall()
{
    // Never remove the virtual endpoint while Windows may still use it as a
    // default device. This guard also protects direct helper invocations that
    // do not come from Inno Setup's normal close-applications sequence.
    if (! prepareForDriverChange()) return 9;
    const auto devices = SetupDiGetClassDevsW(nullptr, nullptr, nullptr, DIGCF_ALLCLASSES);
    if (devices == INVALID_HANDLE_VALUE) return 6;
    bool failed = false;
    SP_DEVINFO_DATA device { sizeof(device) };
    for (DWORD index = 0; SetupDiEnumDeviceInfo(devices, index, &device); ++index)
    {
        if (! deviceHasHardwareId(devices, device)) continue;
        SP_REMOVEDEVICE_PARAMS parameters {};
        parameters.ClassInstallHeader.cbSize = sizeof(SP_CLASSINSTALL_HEADER);
        parameters.ClassInstallHeader.InstallFunction = DIF_REMOVE;
        parameters.Scope = DI_REMOVEDEVICE_GLOBAL;
        parameters.HwProfile = 0;
        if (! SetupDiSetClassInstallParamsW(devices, &device, &parameters.ClassInstallHeader,
                                            sizeof(parameters))
            || ! SetupDiCallClassInstaller(DIF_REMOVE, devices, &device))
            failed = true;
    }
    SetupDiDestroyDeviceInfoList(devices);
    const auto publishedInf = readInstalledInf();
    if (! publishedInf.empty())
    {
        if (SetupUninstallOEMInfW(publishedInf.c_str(), 0, nullptr))
        {
            std::error_code ignored;
            std::filesystem::remove(installedInfRecordPath(), ignored);
        }
        else
        {
            log(L"Driver package removal failed: " + std::to_wstring(GetLastError()));
            failed = true;
        }
    }
    log(failed ? L"Driver device removal failed" : L"Driver device removed");
    return failed ? 7 : 0;
}
}

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int)
{
    const auto comResult = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    int count = 0;
    auto** arguments = CommandLineToArgvW(GetCommandLineW(), &count);
    if (arguments == nullptr || count < 2)
    {
        if (arguments != nullptr) LocalFree(arguments);
        return 1;
    }
    const std::wstring action(arguments[1]);
    int result = 1;
    if (action == L"--install" && count >= 3) result = install(arguments[2]);
    else if (action == L"--uninstall") result = uninstall();
    else if (action == L"--check") result = installationIsOperational(true) ? 0 : 8;
    else if (action == L"--check-device") result = deviceExists() ? 0 : 8;
    LocalFree(arguments);
    if (SUCCEEDED(comResult)) CoUninitialize();
    return result;
}
#else
int main() { return 0; }
#endif
