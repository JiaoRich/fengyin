param(
    [string]$Version = "0.13.0",
    [switch]$SkipInstaller
)

$ErrorActionPreference = "Stop"
$ProjectRoot = Split-Path -Parent $PSScriptRoot
$BuildDir = Join-Path $ProjectRoot "build-windows"
$PackageDir = Join-Path $ProjectRoot "dist\FengYin"
$InstallerDir = Join-Path $ProjectRoot "dist\installer"
$FfmpegPath = Join-Path $ProjectRoot "third_party\ffmpeg\windows\ffmpeg.exe"
$FfmpegPackageDir = Join-Path $PackageDir "tools\ffmpeg"
$NugetPackageDir = Join-Path $ProjectRoot "third_party\nuget-packages"
$WebViewBootstrapperPath = Join-Path $PackageDir "MicrosoftEdgeWebview2Setup.exe"

function Invoke-Checked([string]$StepName, [scriptblock]$Command) {
    & $Command
    if ($LASTEXITCODE -ne 0) {
        throw "$StepName 失败，错误代码：$LASTEXITCODE"
    }
}

if (-not $IsWindows -and $PSVersionTable.PSEdition -eq "Core") {
    throw "此脚本只能在 Windows 电脑上运行。"
}

if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) {
    throw "没有找到 CMake。请先安装 Visual Studio 2022，并勾选‘使用 C++ 的桌面开发’和 CMake 工具。"
}

Write-Host "[1/5] 配置 Windows x64 Release 工程..." -ForegroundColor Cyan
if (-not (Get-ChildItem $NugetPackageDir -Directory -Filter "*Microsoft.Web.WebView2*" -ErrorAction SilentlyContinue)) {
    if (-not (Get-Command nuget -ErrorAction SilentlyContinue)) {
        throw "没有找到 NuGet，无法安装视频界面所需的 Microsoft WebView2 编译组件。"
    }
    Write-Host "正在安装 Microsoft WebView2 编译组件..." -ForegroundColor Cyan
    Invoke-Checked "WebView2 组件安装" {
        nuget install Microsoft.Web.WebView2 -Version 1.0.3485.44 -OutputDirectory $NugetPackageDir -NonInteractive
    }
}
Invoke-Checked "CMake 配置" {
    cmake -S $ProjectRoot -B $BuildDir -G "Visual Studio 17 2022" -A x64 -DBUILD_TESTING=ON `
        "-DJUCE_WEBVIEW2_PACKAGE_LOCATION=$NugetPackageDir"
}

Write-Host "[2/5] 编译风吟和自动测试..." -ForegroundColor Cyan
Invoke-Checked "主程序编译" { cmake --build $BuildDir --config Release --parallel }
Invoke-Checked "激活工具编译" { cmake --build $BuildDir --config Release --target FengYinActivator --parallel }

Write-Host "[3/5] 运行自动测试..." -ForegroundColor Cyan
Invoke-Checked "自动测试" { ctest --test-dir $BuildDir -C Release --output-on-failure }
if (-not (Get-Command python -ErrorAction SilentlyContinue)) {
    throw "没有找到 Python，无法执行发布回归检查。"
}
Invoke-Checked "发布回归检查" { python -m unittest discover -s (Join-Path $ProjectRoot "tests") -p "test_*.py" }

Write-Host "[4/5] 整理安装文件..." -ForegroundColor Cyan
if (Test-Path $PackageDir) {
    Remove-Item -Path $PackageDir -Recurse -Force
}
New-Item -ItemType Directory -Force -Path $PackageDir | Out-Null
$BuiltExe = Join-Path $BuildDir "FengYin_artefacts\Release\FengYin.exe"
if (-not (Test-Path $BuiltExe)) { throw "未找到主程序编译结果：$BuiltExe" }
Copy-Item $BuiltExe (Join-Path $PackageDir "FengYin.exe") -Force
Copy-Item (Join-Path $ProjectRoot "README.md") $PackageDir -Force
Copy-Item (Join-Path $ProjectRoot "THIRD_PARTY_NOTICES.md") $PackageDir -Force
if (Test-Path $FfmpegPath) {
    Copy-Item $FfmpegPath (Join-Path $PackageDir "ffmpeg.exe") -Force
} else {
    Write-Host "正在准备视频伴奏混录组件（FFmpeg LGPL）..." -ForegroundColor Cyan
    $FfmpegRelease = "https://github.com/BtbN/FFmpeg-Builds/releases/download/autobuild-2026-09-15-13-18"
    $FfmpegArchiveName = "ffmpeg-n8.1.2-53-g1005b294ff-win64-lgpl-shared-8.1.zip"
    $FfmpegArchive = Join-Path $env:TEMP $FfmpegArchiveName
    $FfmpegChecksums = Join-Path $env:TEMP "fengyin-ffmpeg-checksums.sha256"
    $FfmpegExtract = Join-Path $env:TEMP "fengyin-ffmpeg-lgpl-8.1"
    Invoke-WebRequest -UseBasicParsing -Uri "$FfmpegRelease/$FfmpegArchiveName" -OutFile $FfmpegArchive
    Invoke-WebRequest -UseBasicParsing -Uri "$FfmpegRelease/checksums.sha256" -OutFile $FfmpegChecksums
    $ExpectedLine = Select-String -Path $FfmpegChecksums -Pattern ([regex]::Escape($FfmpegArchiveName)) | Select-Object -First 1
    if (-not $ExpectedLine) { throw "无法核对 FFmpeg 下载文件。" }
    $ExpectedHash = ($ExpectedLine.Line -split '\s+')[0].ToUpperInvariant()
    $ActualHash = (Get-FileHash -Algorithm SHA256 $FfmpegArchive).Hash.ToUpperInvariant()
    if ($ActualHash -ne $ExpectedHash) { throw "FFmpeg 下载校验失败。" }
    if (Test-Path $FfmpegExtract) { Remove-Item -Path $FfmpegExtract -Recurse -Force }
    Expand-Archive -Path $FfmpegArchive -DestinationPath $FfmpegExtract -Force
    $FfmpegBin = Get-ChildItem $FfmpegExtract -Directory | Select-Object -First 1 | ForEach-Object { Join-Path $_.FullName "bin" }
    if (-not $FfmpegBin -or -not (Test-Path (Join-Path $FfmpegBin "ffmpeg.exe"))) {
        throw "FFmpeg 压缩包结构不正确。"
    }
    New-Item -ItemType Directory -Force -Path $FfmpegPackageDir | Out-Null
    Copy-Item (Join-Path $FfmpegBin "*") $FfmpegPackageDir -Force
    $FfmpegRoot = Split-Path $FfmpegBin -Parent
    Get-ChildItem $FfmpegRoot -File | Where-Object { $_.Name -match 'LICENSE|COPYING|README' } | Copy-Item -Destination $FfmpegPackageDir -Force
}

Write-Host "正在准备 Microsoft Edge WebView2 运行环境安装程序..." -ForegroundColor Cyan
Invoke-WebRequest -UseBasicParsing `
    -Uri "https://go.microsoft.com/fwlink/p/?LinkId=2124703" `
    -OutFile $WebViewBootstrapperPath
if ((Get-Item $WebViewBootstrapperPath).Length -lt 1MB) {
    throw "WebView2 运行环境安装程序下载不完整。"
}

$ExePath = Join-Path $PackageDir "FengYin.exe"
if (-not (Test-Path $ExePath)) {
    throw "未找到编译结果：$ExePath"
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
$ChineseMessages = Join-Path $BuildDir "ChineseSimplified.isl"
if (-not (Test-Path $ChineseMessages)) {
    Write-Host "正在获取 Inno Setup 官方简体中文语言文件..." -ForegroundColor Cyan
    Invoke-WebRequest -UseBasicParsing `
        -Uri "https://raw.githubusercontent.com/jrsoftware/issrc/main/Files/Languages/ChineseSimplified.isl" `
        -OutFile $ChineseMessages
}
if ((Get-Item $ChineseMessages).Length -lt 10000) {
    throw "简体中文语言文件下载不完整，请检查网络后重试。"
}
Invoke-Checked "中文安装包生成" {
    & $Iscc "/DSourceDir=$PackageDir" "/DOutputDir=$InstallerDir" "/DAppVersion=$Version" `
        "/DChineseMessages=$ChineseMessages" (Join-Path $ProjectRoot "installer\FengYin.iss")
}

Write-Host "完成：$InstallerDir\风吟-$Version-Windows-x64.exe" -ForegroundColor Green
