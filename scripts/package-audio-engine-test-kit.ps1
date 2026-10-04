param(
    [Parameter(Mandatory = $true)]
    [string]$AppPackageDir,
    [Parameter(Mandatory = $true)]
    [string]$DriverDir,
    [Parameter(Mandatory = $true)]
    [string]$OutputDir
)

$ErrorActionPreference = "Stop"
$ProjectRoot = Split-Path -Parent $PSScriptRoot
$AppPackageDir = [System.IO.Path]::GetFullPath($AppPackageDir)
$DriverDir = [System.IO.Path]::GetFullPath($DriverDir)
$OutputDir = [System.IO.Path]::GetFullPath($OutputDir)

& (Join-Path $PSScriptRoot "test-fengyin-test-driver.ps1") -DriverDir $DriverDir
if (-not $?) { throw "测试驱动验证失败" }

if (Test-Path $OutputDir) { Remove-Item $OutputDir -Recurse -Force }
New-Item -ItemType Directory -Force -Path $OutputDir | Out-Null
Copy-Item (Join-Path $AppPackageDir "*") $OutputDir -Recurse -Force

$PackagedDriver = Join-Path $OutputDir "test-driver"
New-Item -ItemType Directory -Force -Path $PackagedDriver | Out-Null
foreach ($Name in @("FengYinAudio.inf", "FengYinAudio.cat", "TabletAudioSample.sys", "TabletAudioSample.cer")) {
    Copy-Item (Join-Path $DriverDir $Name) (Join-Path $PackagedDriver $Name) -Force
}

Copy-Item (Join-Path $ProjectRoot "scripts\audio-test-kit\安装测试音频引擎.ps1") $OutputDir -Force
Copy-Item (Join-Path $ProjectRoot "scripts\audio-test-kit\卸载并恢复系统声音.ps1") $OutputDir -Force
Copy-Item (Join-Path $ProjectRoot "scripts\audio-test-kit\开启Windows测试模式.ps1") $OutputDir -Force
Copy-Item (Join-Path $ProjectRoot "scripts\audio-test-kit\关闭Windows测试模式.ps1") $OutputDir -Force
Copy-Item (Join-Path $ProjectRoot "scripts\audio-test-kit\*.cmd") $OutputDir -Force
Copy-Item (Join-Path $ProjectRoot "docs\风吟音频引擎免费实机测试说明.md") (Join-Path $OutputDir "测试说明-请先阅读.md") -Force

Write-Host "免费实机测试包已生成：$OutputDir" -ForegroundColor Green
