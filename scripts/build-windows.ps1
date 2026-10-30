param(
    [string]$Version = "1.1.40",
    [switch]$SkipInstaller
)

$ErrorActionPreference = "Stop"
$env:PYTHONUTF8 = "1"
$ProjectRoot = Split-Path -Parent $PSScriptRoot
$BuildDir = Join-Path $ProjectRoot "build-windows"
$PackageDir = Join-Path $ProjectRoot "dist\FengYin"
$InstallerDir = Join-Path $ProjectRoot "dist\installer"
$FfmpegPath = Join-Path $ProjectRoot "third_party\ffmpeg\windows\ffmpeg.exe"
$FfmpegPackageDir = Join-Path $PackageDir "tools\ffmpeg"
$NugetPackageDir = Join-Path $ProjectRoot "third_party\nuget-packages"
$VlcRoot = Join-Path $ProjectRoot "third_party\vlc"
$RecordingRoot = Join-Path $ProjectRoot "third_party\recording-audio"
$WebViewOfflineInstallerPath = Join-Path $PackageDir "MicrosoftEdgeWebView2RuntimeInstallerX64.exe"
$PreparedFfmpeg = Join-Path $env:TEMP ("fengyin-ffmpeg-" + [guid]::NewGuid().ToString("N"))
$PreparedWebView = Join-Path $env:TEMP ("fengyin-webview-" + [guid]::NewGuid().ToString("N") + ".exe")
$UsePreparedFfmpeg = -not ((Test-Path $FfmpegPath) -and (Test-Path (Join-Path (Split-Path $FfmpegPath) "ffprobe.exe")))

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

if (-not (Get-Command python -ErrorAction SilentlyContinue)) {
    throw "没有找到 Python，无法准备发布依赖。"
}
# Fail inexpensive source regressions before the native build, not after it.
Invoke-Checked "发布源码预检" { python -m unittest discover -s (Join-Path $ProjectRoot "tests") -p "test_*.py" }
Invoke-Checked "LibVLC official runtime and source verification" { python (Join-Path $PSScriptRoot "prepare_vlc.py") $VlcRoot }
Invoke-Checked "Audio-only recording dependencies verification" { python (Join-Path $PSScriptRoot "prepare_audio_recording.py") $RecordingRoot }
# Download before compilation. Never package a partial or unsigned dependency.
for ($attempt = 1; $attempt -le 4; $attempt++) {
    try {
        Write-Host "预检 WebView2 离线运行环境（$attempt/4）..."
        $CachedCiWebView = Join-Path $ProjectRoot 'third_party\ci-webview\WebView2RuntimeInstallerX64.exe'
        if ($env:GITHUB_ACTIONS -eq 'true' -and (Test-Path -LiteralPath $CachedCiWebView)) {
            Copy-Item -LiteralPath $CachedCiWebView -Destination $PreparedWebView -Force
        } else {
            Invoke-WebRequest -UseBasicParsing -TimeoutSec 300 `
                -Uri "https://go.microsoft.com/fwlink/?linkid=2124701" -OutFile $PreparedWebView
        }
        if ((Get-Item $PreparedWebView).Length -lt 100MB) {
            throw "WebView2 下载不完整，实际字节数：$((Get-Item $PreparedWebView).Length)"
        }
        $WebViewSignature = Get-AuthenticodeSignature -FilePath $PreparedWebView
        if ($WebViewSignature.Status -ne 'Valid' -or
            $WebViewSignature.SignerCertificate.Subject -notmatch 'Microsoft Corporation') {
            throw "WebView2 未通过微软数字签名验证。"
        }
        break
    } catch {
        Remove-Item -LiteralPath $PreparedWebView -Force -ErrorAction SilentlyContinue
        if ($attempt -eq 4) { throw }
        Write-Warning "WebView2 预检失败，将重新下载：$_"
        Start-Sleep -Seconds (3 * $attempt)
    }
}
if ($env:GITHUB_ACTIONS -eq 'true') {
    & (Join-Path $PSScriptRoot 'ensure_ci_webview.ps1') -Installer $PreparedWebView
    New-Item -ItemType Directory -Force -Path (Join-Path $BuildDir 'evidence') | Out-Null
    Copy-Item -LiteralPath 'ci-webview-runtime.json' -Destination (Join-Path $BuildDir 'evidence/ci-webview-runtime.json') -Force
}
if ($UsePreparedFfmpeg) {
    Write-Host "预检并校验 FFmpeg 发布依赖..." -ForegroundColor Cyan
    Invoke-Checked "FFmpeg 下载与校验" { python (Join-Path $PSScriptRoot "prepare_ffmpeg.py") $PreparedFfmpeg }
    Invoke-Checked "FFmpeg 可执行性检查" { & (Join-Path $PreparedFfmpeg "ffmpeg.exe") -version }
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
        "-DJUCE_WEBVIEW2_PACKAGE_LOCATION=$NugetPackageDir" "-DFENGYIN_VLC_ROOT=$VlcRoot" "-DFENGYIN_RECORDING_ROOT=$RecordingRoot" -DFENGYIN_ARCHIVED_VIDEO_RECORDING=OFF
}

# Validate the stable Windows BLE MIDI backend before the expensive application build.
Invoke-Checked "Bluetooth MIDI backend preflight build" { cmake --build $BuildDir --config Release --target FengYinBluetoothMidiTests --parallel 4 }
Invoke-Checked "Bluetooth MIDI lifecycle preflight" { ctest --test-dir $BuildDir -C Release -R '^bluetooth_midi_lifecycle$' --output-on-failure }

# The release WAV recorder stays unchanged. GPU recording is a separate helper.
$RecorderFfmpeg = if (-not $UsePreparedFfmpeg) { $FfmpegPath } else { Join-Path $PreparedFfmpeg "ffmpeg.exe" }
Invoke-Checked "Audio-only recorder preflight build" { cmake --build $BuildDir --config Release --target FengYinRecorder --parallel 4 }
$ThirdPartyRecorder = Join-Path $BuildDir "Release\FengYinRecorder.exe"
$ThirdPartyAudio = Join-Path $BuildDir "miniaudio-selftest.wav"
Invoke-Checked "Miniaudio real WAV encoding selftest" {
    if (Test-Path $ThirdPartyAudio) { Remove-Item -LiteralPath $ThirdPartyAudio -Force }
    $NativeProcess = Start-Process -FilePath $ThirdPartyRecorder -ArgumentList @('--audio-selftest', ('"{0}"' -f $ThirdPartyAudio)) -PassThru
    if (-not $NativeProcess.WaitForExit(60000)) { $NativeProcess.Kill(); throw "Audio encoding timed out" }
    $NativeProcess.Refresh()
    $global:LASTEXITCODE = $NativeProcess.ExitCode
    if ($LASTEXITCODE -ne 0 -and (Test-Path "$ThirdPartyAudio.error.txt")) { Get-Content "$ThirdPartyAudio.error.txt" }
    if ($LASTEXITCODE -eq 0 -and -not (Test-Path $ThirdPartyAudio)) { throw "Audio recorder produced no WAV" }
}
Invoke-Checked "Miniaudio actual PCM continuity" { python (Join-Path $PSScriptRoot "verify_system_recording.py") $ThirdPartyAudio }
Invoke-Checked "GPU recorder helper build" { cmake --build $BuildDir --config Release --target FengYinScreenCapture --parallel 4 }
$GpuCaptureExe = Join-Path $BuildDir "Release\FengYinScreenCapture.exe"
Invoke-Checked "GPU recorder actual deferred mux verification" {
    python (Join-Path $PSScriptRoot "test_screen_capture_mux.py") $GpuCaptureExe (Split-Path $RecorderFfmpeg) (Join-Path $BuildDir "evidence\gpu-recorder-mux")
}
# Capability failures on the cloud VM are recorded, not called hardware passes.
# Actual GPU capture + real instrument acceptance remains a target-machine test.
Invoke-Checked "GPU recorder capability report" {
    python (Join-Path $PSScriptRoot "report_screen_capture_capabilities.py") $GpuCaptureExe (Join-Path $BuildDir "evidence\gpu-recorder-capabilities")
}
Invoke-Checked "Recording service preflight build" {
    cmake --build $BuildDir --config Release --target FengYinRecordingServiceTests --parallel 4
}
Invoke-Checked "Recording service dual-file lifecycle preflight" {
    ctest --test-dir $BuildDir -C Release -R '^recording_service_lifecycle$' -V --output-on-failure
}
# Build playback fixtures independently: removing capture must not remove real
# VLC first-frame / decoded PCM / lifecycle acceptance.
Invoke-Checked "Asynchronous video file selector preflight build" {
    cmake --build $BuildDir --config Release --target FengYinVideoChooserTests --parallel 4
}
Invoke-Checked "Real video selector cancel/reopen/lifecycle check" {
    ctest --test-dir $BuildDir -C Release -R '^video_file_selector_lifecycle$' -V --output-on-failure
}
$VlcFixture = Join-Path $BuildDir "vlc-playback-fixture.mp4"
Invoke-Checked "LibVLC fixture preparation" {
    & $RecorderFfmpeg -hide_banner -loglevel error -nostdin -y -f lavfi -i "color=c=red:s=1920x1080:r=24" -f lavfi -i "sine=frequency=440:sample_rate=48000" -vf "drawbox=x=0:y=540:w=1920:h=540:color=blue:t=fill" -t 12 -c:v mpeg4 -q:v 3 -c:a aac -b:a 192k -ac 2 $VlcFixture
}
Invoke-Checked "LibVLC integration preflight build" { cmake --build $BuildDir --config Release --target FengYinVlcTests --parallel 4 }
Invoke-Checked "LibVLC real playback and lifecycle check" {
    ctest --test-dir $BuildDir -C Release -R '^vlc_real_playback$' -V --output-on-failure
}
Invoke-Checked "Real WebView2 and VLC native-input preflight build" {
    cmake --build $BuildDir --config Release --target FengYinVlcWebViewCheck --parallel 4
}
Invoke-Checked "Real WebView2 and VLC native-input interaction" {
    ctest --test-dir $BuildDir -C Release -R '^vlc_webview_interaction$' -V --output-on-failure
}

Write-Host "[2/5] 编译风吟和自动测试..." -ForegroundColor Cyan
Invoke-Checked "主程序编译" { cmake --build $BuildDir --config Release --parallel }
Invoke-Checked "激活工具编译" { cmake --build $BuildDir --config Release --target FengYinActivator --parallel }

Write-Host "[3/5] 运行自动测试..." -ForegroundColor Cyan
Invoke-Checked "自动测试" { ctest --test-dir $BuildDir -C Release --output-on-failure }
$ProxyInput = Join-Path $BuildDir "video-proxy-input.mp4"
$ProxyOutput = Join-Path $BuildDir "verified-proxy.mp4"
Invoke-Checked "Video proxy 60fps input preparation" {
    & $RecorderFfmpeg -hide_banner -loglevel error -nostdin -y -i $VlcFixture -t 2 -an -r 60 -c:v mpeg4 -q:v 4 $ProxyInput
}
Invoke-Checked "Native idle video preparation and cache reuse" {
    & (Join-Path $BuildDir "Release\FengYinRecordingTests.exe") $RecorderFfmpeg $ProxyInput
}
Invoke-Checked "Video proxy decoded frame and duration check" {
    python (Join-Path $PSScriptRoot "verify_video_proxy.py") $RecorderFfmpeg $ProxyOutput
}
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
$AudioEngineExe = Join-Path $BuildDir "Release\FengYinAudioEngine.exe"
$AudioWatchdogExe = Join-Path $BuildDir "Release\FengYinAudioWatchdog.exe"
if (-not (Test-Path $AudioEngineExe)) { throw "未找到独立音频引擎：$AudioEngineExe" }
if (-not (Test-Path $AudioWatchdogExe)) { throw "未找到音频恢复守护程序：$AudioWatchdogExe" }
Copy-Item $AudioEngineExe (Join-Path $PackageDir "FengYinAudioEngine.exe") -Force
Copy-Item $AudioWatchdogExe (Join-Path $PackageDir "FengYinAudioWatchdog.exe") -Force
Copy-Item $ThirdPartyRecorder (Join-Path $PackageDir "FengYinRecorder.exe") -Force
Copy-Item $GpuCaptureExe (Join-Path $PackageDir "FengYinScreenCapture.exe") -Force
New-Item -ItemType Directory -Force -Path (Join-Path $PackageDir "components\Recording") | Out-Null
Copy-Item (Join-Path $RecordingRoot "licenses\*") (Join-Path $PackageDir "components\Recording") -Force
Copy-Item (Join-Path $RecordingRoot "versions.json") (Join-Path $PackageDir "components\Recording") -Force
# The public installer uses VB-CABLE. Never package or invoke the experimental driver.
Copy-Item (Join-Path $ProjectRoot "README.md") $PackageDir -Force
Copy-Item (Join-Path $ProjectRoot "THIRD_PARTY_NOTICES.md") $PackageDir -Force
New-Item -ItemType Directory -Force -Path (Join-Path $PackageDir "components\BluetoothMIDI") | Out-Null
Copy-Item (Join-Path $ProjectRoot "third_party\ble-midi\LICENSE.txt") (Join-Path $PackageDir "components\BluetoothMIDI") -Force
& (Join-Path $PSScriptRoot "prepare-vbcable.ps1") -Destination (Join-Path $PackageDir "components\VB-CABLE")
& (Join-Path $PSScriptRoot "prepare-asio4all.ps1") -Destination (Join-Path $PackageDir "components\ASIO4ALL")
& (Join-Path $PSScriptRoot "prepare-fresh-air.ps1") -Destination (Join-Path $PackageDir "components\Fresh Air")
$MagicDestination = Join-Path $PackageDir "components\magic.CURVE"
Copy-Item -LiteralPath $VlcRoot -Destination (Join-Path $PackageDir "components\VLC") -Recurse -Force
Invoke-Checked "Final private VLC plugin index" {
    python (Join-Path $PSScriptRoot "prepare_vlc_cache.py") (Join-Path $PackageDir "components\VLC")
}
Copy-Item (Join-Path $ProjectRoot "installer\InstallNotice.txt") (Join-Path $PackageDir "InstallNotice.txt") -Force
if ((Get-FileHash (Join-Path $ProjectRoot "assets\components\magic.CURVE-1.0.2-Windows-Setup.exe") -Algorithm SHA256).Hash -ne '5AEBF446018D5F8FCE87F332E9D8A01194A98E02536DE132BF1C64939417BDBD') { throw 'magic.CURVE installer SHA-256 mismatch' }
New-Item -ItemType Directory -Path $MagicDestination -Force | Out-Null
Copy-Item (Join-Path $ProjectRoot "assets\components\magic.CURVE-1.0.2-Windows-Setup.exe") (Join-Path $MagicDestination "magic.CURVE-1.0.2-Windows-Setup.exe") -Force
Copy-Item (Join-Path $ProjectRoot "installer\AudioComponents.txt") $PackageDir -Force
if (-not $UsePreparedFfmpeg) {
    Copy-Item $FfmpegPath (Join-Path $PackageDir "ffmpeg.exe") -Force
    Copy-Item (Join-Path (Split-Path $FfmpegPath) "ffprobe.exe") (Join-Path $PackageDir "ffprobe.exe") -Force
} else {
    Write-Host "正在准备视频伴奏混录组件（FFmpeg LGPL）..." -ForegroundColor Cyan
    New-Item -ItemType Directory -Force -Path $FfmpegPackageDir | Out-Null
    Copy-Item (Join-Path $PreparedFfmpeg "*") $FfmpegPackageDir -Force
}

Write-Host "正在准备 Microsoft Edge WebView2 x64 离线运行环境..." -ForegroundColor Cyan
Copy-Item -LiteralPath $PreparedWebView -Destination $WebViewOfflineInstallerPath -Force
if ((Get-Item $WebViewOfflineInstallerPath).Length -lt 100MB) {
    throw "WebView2 x64 离线运行环境下载不完整。"
}
$WebViewSignature = Get-AuthenticodeSignature -FilePath $WebViewOfflineInstallerPath
if ($WebViewSignature.Status -ne 'Valid' -or
    $WebViewSignature.SignerCertificate.Subject -notmatch 'Microsoft Corporation') {
    throw "WebView2 x64 离线运行环境未通过微软数字签名验证。"
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
        "/DChineseMessages=$ChineseMessages" `
        (Join-Path $ProjectRoot "installer\FengYin.iss")
}

Write-Host "完成：$InstallerDir\风吟-$Version-Windows-x64.exe" -ForegroundColor Green
