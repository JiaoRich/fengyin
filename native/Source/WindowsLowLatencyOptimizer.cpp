#include "WindowsLowLatencyOptimizer.h"

#if JUCE_WINDOWS
#include <windows.h>
#include <shellapi.h>
#include <setupapi.h>
#endif

namespace
{
const char* optimiserScript = R"PS1(param(
    [ValidateSet('Apply','Restore')][string]$Mode,
    [Parameter(Mandatory=$true)][string]$WorkDir
)
$ErrorActionPreference = 'Stop'
$ResultPath = Join-Path $WorkDir 'result.json'
$ManifestPath = Join-Path $WorkDir 'backup-manifest.json'
$BackupDir = Join-Path $WorkDir 'driver-backup'

function Write-Result([string]$State, [string]$Code, [bool]$CanRestore, [bool]$RestartRequired, [string]$DeviceName = '') {
    [ordered]@{ state=$State; code=$Code; canRestore=$CanRestore; restartRequired=$RestartRequired; deviceName=$DeviceName } |
        ConvertTo-Json | Set-Content -LiteralPath $ResultPath -Encoding UTF8
}
function Invoke-Pnp([string[]]$Arguments) {
    $output = & "$env:windir\System32\pnputil.exe" @Arguments 2>&1
    if ($LASTEXITCODE -ne 0) { throw ($output -join "`n") }
    return ($output -join "`n")
}
function Get-HdaDrivers {
    @(Get-CimInstance Win32_PnPSignedDriver | Where-Object {
        $_.DeviceClass -eq 'MEDIA' -and $_.DeviceID -like 'HDAUDIO\FUNC_01*'
    })
}

try {
    New-Item -ItemType Directory -Force -Path $WorkDir | Out-Null
    if ($Mode -eq 'Restore') {
        if (-not (Test-Path -LiteralPath $ManifestPath)) { Write-Result 'error' 'no-backup' $false $false; exit 2 }
        $manifest = Get-Content -LiteralPath $ManifestPath -Raw | ConvertFrom-Json
        $infFiles = @(Get-ChildItem -LiteralPath $BackupDir -Filter '*.inf' -Recurse -File)
        if ($infFiles.Count -eq 0) { Write-Result 'error' 'backup-missing' $false $false; exit 3 }
        foreach ($inf in $infFiles) { Invoke-Pnp @('/add-driver', $inf.FullName, '/install') | Out-Null }
        Invoke-Pnp @('/scan-devices') | Out-Null
        if ($manifest.powerScheme) { & "$env:windir\System32\powercfg.exe" /setactive $manifest.powerScheme | Out-Null }
        Write-Result 'success' 'restore-complete' $true $true ([string]$manifest.deviceName)
        exit 0
    }

    $conflicts = @(Get-Service -ErrorAction SilentlyContinue | Where-Object {
        $_.Name -match 'Nahimic|A-Volute|Dolby' -or $_.DisplayName -match 'Nahimic|A-Volute|Dolby'
    })
    if ($conflicts.Count -gt 0) { Write-Result 'error' 'enhancement-conflict' (Test-Path $ManifestPath) $false; exit 4 }

    $eligible = @(Get-HdaDrivers | Where-Object {
        ($_.DeviceName + ' ' + $_.Manufacturer + ' ' + $_.DriverProviderName) -match 'Realtek|Senary|C-Media'
    })
    if ($eligible.Count -ne 1) {
        Write-Result 'error' ($(if ($eligible.Count -eq 0) {'no-eligible-device'} else {'multiple-devices'})) (Test-Path $ManifestPath) $false
        exit 5
    }
    $device = $eligible[0]
    if (-not ($device.InfName -match '^oem\d+\.inf$')) { Write-Result 'error' 'invalid-driver-package' (Test-Path $ManifestPath) $false $device.DeviceName; exit 6 }
    if (-not (Test-Path "$env:windir\INF\hdaudio.inf")) { Write-Result 'error' 'inbox-driver-missing' (Test-Path $ManifestPath) $false $device.DeviceName; exit 7 }

    if (Test-Path -LiteralPath $BackupDir) { Remove-Item -LiteralPath $BackupDir -Recurse -Force }
    New-Item -ItemType Directory -Force -Path $BackupDir | Out-Null
    Invoke-Pnp @('/export-driver', [string]$device.InfName, $BackupDir) | Out-Null
    if (@(Get-ChildItem -LiteralPath $BackupDir -Filter '*.inf' -Recurse -File).Count -eq 0) {
        Write-Result 'error' 'backup-failed' $false $false $device.DeviceName; exit 8
    }

    $schemeText = (& "$env:windir\System32\powercfg.exe" /getactivescheme 2>$null) -join ' '
    $scheme = ([regex]::Match($schemeText, '[0-9a-fA-F-]{36}')).Value
    [ordered]@{
        deviceName=[string]$device.DeviceName; deviceId=[string]$device.DeviceID;
        infName=[string]$device.InfName; provider=[string]$device.DriverProviderName;
        driverVersion=[string]$device.DriverVersion; powerScheme=$scheme
    } | ConvertTo-Json | Set-Content -LiteralPath $ManifestPath -Encoding UTF8

    # High performance is reversible and avoids clock throttling during real-time audio.
    & "$env:windir\System32\powercfg.exe" /setactive SCHEME_MIN 2>$null | Out-Null

    # Removing the ranked OEM package lets Plug and Play bind Microsoft's signed
    # inbox hdaudio.inf. The exported package above is the rollback source.
    Invoke-Pnp @('/delete-driver', [string]$device.InfName, '/uninstall', '/force') | Out-Null
    Invoke-Pnp @('/scan-devices') | Out-Null
    Start-Sleep -Seconds 3
    $after = @(Get-HdaDrivers | Where-Object { $_.DeviceID -eq $device.DeviceID }) | Select-Object -First 1
    if (-not $after -or ($after.DriverProviderName -notmatch 'Microsoft' -and $after.InfName -ne 'hdaudio.inf')) {
        foreach ($inf in @(Get-ChildItem -LiteralPath $BackupDir -Filter '*.inf' -Recurse -File)) {
            Invoke-Pnp @('/add-driver', $inf.FullName, '/install') | Out-Null
        }
        Invoke-Pnp @('/scan-devices') | Out-Null
        Write-Result 'error' 'generic-driver-not-selected' $true $true $device.DeviceName
        exit 9
    }
    Write-Result 'success' 'optimisation-complete' $true $true $device.DeviceName
} catch {
    Write-Result 'error' 'unexpected-error' (Test-Path $ManifestPath) $false
    exit 10
}
)PS1";

#if JUCE_WINDOWS
juce::String resultMessage(const juce::String& code, const juce::String& device)
{
    const auto suffix = device.isNotEmpty() ? " (" + device + ")" : juce::String();
    if (code == "optimisation-complete") return juce::String::fromUTF8("系统驱动已切换，请重启电脑后打开风吟完成延迟测试。") + suffix;
    if (code == "restore-complete") return juce::String::fromUTF8("原声卡驱动已恢复，请重启电脑。") + suffix;
    if (code == "no-backup" || code == "backup-missing") return juce::String::fromUTF8("没有找到可恢复的原驱动备份。");
    if (code == "enhancement-conflict") return juce::String::fromUTF8("检测到 Dolby 或 Nahimic 等音效服务。请先卸载这类音效后再优化，否则可能无声或爆音。");
    if (code == "no-eligible-device") return juce::String::fromUTF8("未找到可安全切换的 Realtek / Senary / C-Media 板载声卡。");
    if (code == "multiple-devices") return juce::String::fromUTF8("检测到多个候选板载声卡，为避免更改错设备，已取消自动切换。");
    if (code == "backup-failed") return juce::String::fromUTF8("原声卡驱动备份失败，已中止，未更改系统驱动。");
    if (code == "inbox-driver-missing") return juce::String::fromUTF8("系统缺少微软 High Definition Audio 驱动，已中止。");
    if (code == "generic-driver-not-selected") return juce::String::fromUTF8("微软通用驱动未能正常绑定，已尝试恢复原驱动；请重启后检查声音。");
    if (code == "unexpected-error") return juce::String::fromUTF8("系统优化未完成，未删除备份，可使用“恢复原驱动”。");
    return juce::String::fromUTF8("尚未运行超级低延迟优化。");
}
#endif
}

namespace fengyin
{
juce::File WindowsLowLatencyOptimizer::getWorkDirectory()
{
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("FengYin").getChildFile("SystemAudioOptimisation");
}

juce::File WindowsLowLatencyOptimizer::getScriptFile() { return getWorkDirectory().getChildFile("optimise-audio.ps1"); }
juce::File WindowsLowLatencyOptimizer::getResultFile() { return getWorkDirectory().getChildFile("result.json"); }
juce::File WindowsLowLatencyOptimizer::getManifestFile() { return getWorkDirectory().getChildFile("backup-manifest.json"); }

WindowsLowLatencyStatus WindowsLowLatencyOptimizer::getStatus() const
{
    WindowsLowLatencyStatus status;
   #if JUCE_WINDOWS
    status.supported = true;
    const GUID mediaClass { 0x4d36e96c, 0xe325, 0x11ce, {0xbf,0xc1,0x08,0x00,0x2b,0xe1,0x03,0x18} };
    auto devices = SetupDiGetClassDevsW(&mediaClass, nullptr, nullptr, DIGCF_PRESENT);
    if (devices != INVALID_HANDLE_VALUE)
    {
        SP_DEVINFO_DATA info {}; info.cbSize = sizeof(info);
        for (DWORD index = 0; SetupDiEnumDeviceInfo(devices, index, &info); ++index)
        {
            wchar_t id[2048] {}, name[512] {}, manufacturer[512] {};
            if (! SetupDiGetDeviceInstanceIdW(devices, &info, id, 2048, nullptr)) continue;
            if (! juce::String(id).startsWithIgnoreCase("HDAUDIO\\FUNC_01")) continue;
            DWORD type = 0, bytes = sizeof(name);
            if (! SetupDiGetDeviceRegistryPropertyW(devices, &info, SPDRP_FRIENDLYNAME, &type,
                                                    reinterpret_cast<PBYTE>(name), bytes, nullptr))
            {
                bytes = sizeof(name);
                SetupDiGetDeviceRegistryPropertyW(devices, &info, SPDRP_DEVICEDESC, &type,
                                                  reinterpret_cast<PBYTE>(name), bytes, nullptr);
            }
            bytes = sizeof(manufacturer);
            SetupDiGetDeviceRegistryPropertyW(devices, &info, SPDRP_MFG, &type,
                                              reinterpret_cast<PBYTE>(manufacturer), bytes, nullptr);
            const auto identity = juce::String(name) + " " + juce::String(manufacturer);
            if (identity.containsIgnoreCase("Realtek") || identity.containsIgnoreCase("Senary")
                || identity.containsIgnoreCase("C-Media"))
            {
                status.eligibleDeviceFound = true;
                status.deviceName = name;
                break;
            }
        }
        SetupDiDestroyDeviceInfoList(devices);
    }
    status.canRestore = getManifestFile().existsAsFile();
    if (const auto json = juce::JSON::parse(getResultFile().loadFileAsString()); json.isObject())
    {
        status.state = json.getProperty("state", "idle").toString();
        status.canRestore = static_cast<bool>(json.getProperty("canRestore", status.canRestore));
        status.restartRequired = static_cast<bool>(json.getProperty("restartRequired", false));
        const auto resultDevice = json.getProperty("deviceName", {}).toString();
        if (resultDevice.isNotEmpty()) status.deviceName = resultDevice;
        status.message = resultMessage(json.getProperty("code", {}).toString(), status.deviceName);
    }
    else
    {
        status.message = status.eligibleDeviceFound
            ? juce::String::fromUTF8("已检测到可优化的板载声卡：") + status.deviceName
            : juce::String::fromUTF8("当前未检测到适用的板载声卡。");
    }
   #else
    status.message = juce::String::fromUTF8("该功能仅用于 Windows 板载声卡。");
   #endif
    return status;
}

bool WindowsLowLatencyOptimizer::writeEmbeddedScript()
{
    auto directory = getWorkDirectory();
    if (! directory.createDirectory()) return false;
    return getScriptFile().replaceWithText(optimiserScript, false, false, "\r\n");
}

bool WindowsLowLatencyOptimizer::launchElevated(const juce::String& mode)
{
   #if JUCE_WINDOWS
    if (! writeEmbeddedScript()) return false;
    getResultFile().deleteFile();
    const auto parameters = "-NoProfile -NonInteractive -ExecutionPolicy Bypass -File \""
        + getScriptFile().getFullPathName() + "\" -Mode " + mode + " -WorkDir \""
        + getWorkDirectory().getFullPathName() + "\"";
    SHELLEXECUTEINFOW info {}; info.cbSize = sizeof(info); info.fMask = SEE_MASK_NOCLOSEPROCESS;
    info.lpVerb = L"runas"; info.lpFile = L"powershell.exe";
    const auto wide = parameters.toWideCharPointer(); info.lpParameters = wide;
    info.nShow = SW_SHOWNORMAL;
    if (! ShellExecuteExW(&info)) return false;
    if (info.hProcess != nullptr) CloseHandle(info.hProcess);
    return true;
   #else
    juce::ignoreUnused(mode);
    return false;
   #endif
}

bool WindowsLowLatencyOptimizer::launchOptimisation() { return launchElevated("Apply"); }
bool WindowsLowLatencyOptimizer::launchRestore() { return launchElevated("Restore"); }
}
