#if defined(_WIN32)
#include <windows.h>
#include <newdev.h>
#include <setupapi.h>
#include <shellapi.h>

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace
{
constexpr wchar_t hardwareId[] = L"Root\\FengYinAudioEngine";

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

bool createRootDevice(const std::filesystem::path& infPath)
{
    GUID classGuid {};
    wchar_t className[MAX_CLASS_NAME_LEN] {};
    if (! SetupDiGetINFClassW(infPath.c_str(), &classGuid, className, MAX_CLASS_NAME_LEN, nullptr))
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

int install(const std::filesystem::path& suppliedInf)
{
    std::error_code ignored;
    const auto infPath = std::filesystem::absolute(suppliedInf, ignored);
    if (ignored || ! std::filesystem::is_regular_file(infPath))
    {
        log(L"INF not found: " + suppliedInf.wstring());
        return 2;
    }
    wchar_t copiedInf[MAX_PATH] {};
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
    if (! deviceExists() && ! createRootDevice(infPath)) return 4;
    BOOL reboot = FALSE;
    if (! UpdateDriverForPlugAndPlayDevicesW(nullptr, hardwareId, infPath.c_str(),
                                              INSTALLFLAG_FORCE, &reboot))
    {
        log(L"UpdateDriverForPlugAndPlayDevices failed: " + std::to_wstring(GetLastError()));
        return 5;
    }
    log(reboot ? L"Driver installed; reboot required" : L"Driver installed");
    return reboot ? 3010 : 0;
}

int uninstall()
{
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
    log(failed ? L"Driver device removal failed" : L"Driver device removed");
    return failed ? 7 : 0;
}
}

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int)
{
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
    else if (action == L"--check") result = deviceExists() ? 0 : 8;
    LocalFree(arguments);
    return result;
}
#else
int main() { return 0; }
#endif
