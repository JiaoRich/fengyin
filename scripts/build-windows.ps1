param(
    [string]$Version = "0.1.0",
    [switch]$SkipInstaller
)

$ErrorActionPreference = "Stop"
$ProjectRoot = Split-Path -Parent $PSScriptRoot
$BuildDir = Join-Path $ProjectRoot "build-windows"
$PackageDir = Join-Path $ProjectRoot "dist\FengYin"
$InstallerDir = Join-Path $ProjectRoot "dist\installer"
$FfmpegPath = Join-Path $ProjectRoot "third_party\ffmpeg\windows\ffmpeg.exe"

if (-not $IsWindows -and $PSVersionTable.PSEdition -eq "Core") {
    throw "此脚本只能在 Windows 电脑上运行。"
}

if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) {
    throw "没有找到 CMake。请先安装 Visual Studio 2022，并勾选‘使用 C++ 的桌面开发’和 CMake 工具。"
}

Write-Host "[1/5] 配置 Windows x64 Release 工程..." -ForegroundColor Cyan
cmake -S $ProjectRoot -B $BuildDir -G "Visual Studio 17 2022" -A x64 -DBUILD_TESTING=ON

Write-Host "[2/5] 编译风吟和自动测试..." -ForegroundColor Cyan
cmake --build $BuildDir --config Release --parallel
cmake --build $BuildDir --config Release --target FengYinActivator --parallel

Write-Host "[3/5] 运行自动测试..." -ForegroundColor Cyan
ctest --test-dir $BuildDir -C Release --output-on-failure

Write-Host "[4/5] 整理安装文件..." -ForegroundColor Cyan
cmake --install $BuildDir --config Release --prefix $PackageDir

$ExePath = Join-Path $PackageDir "FengYin.exe"
if (-not (Test-Path $ExePath)) {
    throw "未找到编译结果：$ExePath"
}

if (-not (Test-Path $FfmpegPath)) {
    Write-Warning "未放入 ffmpeg.exe：视频仍可播放，但部分视频的伴奏混录功能不可用。"
}

# 单独整理店主工具，绝不放进客户安装目录。
$OwnerToolDir = Join-Path $ProjectRoot "dist\店主激活工具"
$ActivatorExe = Join-Path $BuildDir "FengYinActivator_artefacts\Release\风吟激活码工具.exe"
if (-not (Test-Path $ActivatorExe)) {
    throw "未找到激活工具编译结果：$ActivatorExe"
}
New-Item -ItemType Directory -Force -Path $OwnerToolDir | Out-Null
Copy-Item $ActivatorExe (Join-Path $OwnerToolDir "风吟激活码工具.exe") -Force
$PrivateKeyPath = Join-Path $ProjectRoot "private\license-private-key.txt"
if (Test-Path $PrivateKeyPath) {
    Copy-Item $PrivateKeyPath (Join-Path $OwnerToolDir "license-private-key.txt") -Force
} else {
    Write-Warning "未找到激活私钥；激活工具已生成，但需要手工选择私钥文件。"
}

& (Join-Path $PSScriptRoot "check-release.ps1") -PackageDir $PackageDir

if ($SkipInstaller) {
    Write-Host "已生成免安装测试版：$PackageDir" -ForegroundColor Green
    exit 0
}

$IsccCandidates = @(
    "${env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe",
    "$env:ProgramFiles\Inno Setup 6\ISCC.exe"
)
$Iscc = $IsccCandidates | Where-Object { Test-Path $_ } | Select-Object -First 1
if (-not $Iscc) {
    throw "没有找到 Inno Setup 6。安装后重新运行本脚本，或加 -SkipInstaller 只生成测试版。"
}

New-Item -ItemType Directory -Force -Path $InstallerDir | Out-Null
Write-Host "[5/5] 生成中文安装程序..." -ForegroundColor Cyan
& $Iscc "/DSourceDir=$PackageDir" "/DOutputDir=$InstallerDir" "/DAppVersion=$Version" (Join-Path $ProjectRoot "installer\FengYin.iss")

Write-Host "完成：$InstallerDir\风吟-$Version-Windows-x64.exe" -ForegroundColor Green
