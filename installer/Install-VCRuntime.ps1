param(
    [Parameter(Mandatory=$true)][string]$PackageDirectory,
    [string]$LogDirectory = "$env:ProgramData\FengYin\InstallLogs",
    [switch]$VerifyOnly
)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'VCRuntimePolicy.ps1')
$run = $null

function Read-InstalledVCRuntime {
    $versions = @()
    foreach ($view in @([Microsoft.Win32.RegistryView]::Registry64, [Microsoft.Win32.RegistryView]::Registry32)) {
        $base = [Microsoft.Win32.RegistryKey]::OpenBaseKey([Microsoft.Win32.RegistryHive]::LocalMachine,$view)
        try {
            $key = $base.OpenSubKey('SOFTWARE\Microsoft\VisualStudio\14.0\VC\Runtimes\x64')
            if ($null -eq $key) { continue }
            try {
                if ([int]$key.GetValue('Installed',0) -ne 1) { continue }
                $v = [version](([string]$key.GetValue('Version','')).TrimStart('v','V'))
                $versions += $v
            } finally { $key.Dispose() }
        } finally { $base.Dispose() }
    }
    return $versions | Sort-Object -Descending | Select-Object -First 1
}

function Test-VCRuntimeFiles {
    foreach ($name in @('vcruntime140.dll','vcruntime140_1.dll','msvcp140.dll')) {
        $path = Join-Path ([Environment]::SystemDirectory) $name
        if (-not (Test-Path -LiteralPath $path)) { return $false }
        $info = [Diagnostics.FileVersionInfo]::GetVersionInfo($path)
        $version = [version]::new($info.FileMajorPart,$info.FileMinorPart,$info.FileBuildPart,$info.FilePrivatePart)
        if ($version -lt $required) { return $false }
        # Check real system-loader resolution, not just a registry/directory name.
        $module = [FengYin.VCPrerequisite]::LoadLibraryEx($path,[IntPtr]::Zero,0x800)
        if ($module -eq [IntPtr]::Zero) { return $false }
        [void][FengYin.VCPrerequisite]::FreeLibrary($module)
    }
    return $true
}

try {
    if (-not $VerifyOnly) {
        New-Item -ItemType Directory -Force -Path $LogDirectory | Out-Null
        $run = Join-Path $LogDirectory ('vc-runtime-'+(Get-Date -Format 'yyyyMMdd-HHmmss')+'-'+[guid]::NewGuid().ToString('N').Substring(0,6))
    }
    $manifest = Get-Content -LiteralPath (Join-Path $PackageDirectory 'vc-runtime.json') -Raw | ConvertFrom-Json
    $required = [version]$manifest.version
    $exe = Join-Path $PackageDirectory $manifest.file
    if (-not [Environment]::Is64BitProcess) { throw 'Required: 64-bit Windows PowerShell.' }
    if ((Get-FileHash -LiteralPath $exe -Algorithm SHA256).Hash -ne $manifest.sha256) { throw 'VC++ installer SHA256 mismatch.' }
    $signature = Get-AuthenticodeSignature -LiteralPath $exe
    if ($signature.Status -ne 'Valid' -or $signature.SignerCertificate.Subject -notmatch 'Microsoft Corporation') { throw 'VC++ installer Microsoft signature verification failed.' }
    $info = (Get-Item -LiteralPath $exe).VersionInfo
    $binaryVersion = [version]::new($info.FileMajorPart,$info.FileMinorPart,$info.FileBuildPart,$info.FilePrivatePart)
    if ($binaryVersion -ne $required) { throw 'VC++ installer version mismatch.' }
    if ($VerifyOnly) { Write-Output ('Verified Microsoft VC++ '+$required); exit 0 }
    Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
namespace FengYin {
 public static class VCPrerequisite {
  [DllImport("kernel32.dll", CharSet=CharSet.Unicode, SetLastError=true)] public static extern IntPtr LoadLibraryEx(string path,IntPtr reserved,uint flags);
  [DllImport("kernel32.dll", SetLastError=true)] public static extern bool FreeLibrary(IntPtr module);
 }
}
'@
    $installed = Read-InstalledVCRuntime
    $healthy = Test-VCRuntimeFiles
    $action = Get-VCRuntimeAction $required $installed $healthy
    $report = [ordered]@{required=$required.ToString();installedBefore=[string]$installed;healthyBefore=$healthy;action=$action;result='pending'}
    $report | ConvertTo-Json | Set-Content -LiteralPath ($run+'.json') -Encoding UTF8
    if ($action -eq 'newer-needs-repair') {
        throw 'A newer VC++ runtime is installed but damaged. Repair that installed version in Windows Installed Apps; the bundled older runtime will not replace it.'
    }
    $code = 0
    if ($action -ne 'skip') {
        $arguments = '/'+$action+' /quiet /norestart /log "'+$run+'.log"'
        $process = Start-Process -FilePath $exe -ArgumentList $arguments -PassThru
        if (-not $process.WaitForExit(600000)) {
            throw 'VC++ preparation exceeded 10 minutes. The Microsoft installer was not forcibly terminated; wait for it to finish before retrying. See its install log.'
        }
        $process.Refresh()
        $code = $process.ExitCode
    }
    $after = Read-InstalledVCRuntime
    $registered = $null -ne $after -and $after -ge $required
    $verified = $registered -and (Test-VCRuntimeFiles)
    $result = Get-VCRuntimeExitAction $code $verified $registered
    $report['exitCode']=$code;$report['installedAfter']=[string]$after;$report['verified']=$verified;$report['result']=$result
    $report | ConvertTo-Json | Set-Content -LiteralPath ($run+'.json') -Encoding UTF8
    if ($result -eq 'restart') { exit 3010 }
    if ($result -eq 'restart-before-retry') { exit 3011 }
    if ($result -ne 'complete') { throw ('VC++ prerequisite failed: exit='+$code+', registered='+$registered+', verified='+$verified) }
    exit 0
} catch {
    if ($run) { $_ | Out-String | Set-Content -LiteralPath ($run+'-error.txt') -Encoding UTF8 }
    Write-Error -ErrorAction Continue $_
    exit 1
}
