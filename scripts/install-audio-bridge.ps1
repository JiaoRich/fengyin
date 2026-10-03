param([string]$ResultFile = "")

$ErrorActionPreference = "Stop"
$ProgressPreference = "SilentlyContinue"
$work = Join-Path $env:TEMP "FengYin-AudioBridge"
$vmZip = Join-Path $work "VoicemeeterSetup_v2130.zip"
$vmDir = Join-Path $work "Voicemeeter"
$asioExe = Join-Path $work "ASIO4ALL_2_15_SCN.exe"
$scriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$bundledDir = Join-Path $scriptRoot "packages"

function Set-Result([string]$state, [string]$message) {
    if ($ResultFile) {
        @{ state = $state; message = $message; updatedAt = (Get-Date).ToString("o") } |
            ConvertTo-Json | Set-Content -Path $ResultFile -Encoding UTF8
    }
}

function Download-Verified([string]$uri, [string]$path, [string]$sha256) {
    if (Test-Path $path) {
        if ((Get-FileHash -Algorithm SHA256 $path).Hash.ToLowerInvariant() -eq $sha256) { return }
        Remove-Item $path -Force
    }
    Invoke-WebRequest -UseBasicParsing -Uri $uri -OutFile $path
    $actual = (Get-FileHash -Algorithm SHA256 $path).Hash.ToLowerInvariant()
    if ($actual -ne $sha256) { throw "下载文件校验失败：$path" }
}

try {
    New-Item -ItemType Directory -Force -Path $work | Out-Null
    Set-Result "running" "正在下载官方桥接组件…"
    $bundledVm = Join-Path $bundledDir "VoicemeeterSetup_v2130.zip"
    $bundledAsio = Join-Path $bundledDir "ASIO4ALL_2_15_SCN.exe"
    if (Test-Path $bundledVm) { Copy-Item $bundledVm $vmZip -Force }
    else {
        Download-Verified "https://download.vb-audio.com/Download_CABLE/VoicemeeterSetup_v2130.zip" `
            $vmZip "ee8b1f6cd4728233c8ae52897128c9af02367c091b88f55a1996d424e04d3965"
    }
    if (Test-Path $bundledAsio) { Copy-Item $bundledAsio $asioExe -Force }
    else {
        Download-Verified "https://asio4all.org/downloads/ASIO4ALL_2_15_SCN.exe" `
            $asioExe "7921f771d69ec687dce2718edcaeead4b5ea089deb25dc437065f0554da0868e"
    }
    if ((Get-FileHash -Algorithm SHA256 $vmZip).Hash.ToLowerInvariant() -ne "ee8b1f6cd4728233c8ae52897128c9af02367c091b88f55a1996d424e04d3965") {
        throw "VoiceMeeter 安装文件校验失败"
    }
    if ((Get-FileHash -Algorithm SHA256 $asioExe).Hash.ToLowerInvariant() -ne "7921f771d69ec687dce2718edcaeead4b5ea089deb25dc437065f0554da0868e") {
        throw "ASIO4ALL 安装文件校验失败"
    }

    if (Test-Path $vmDir) { Remove-Item $vmDir -Recurse -Force }
    Expand-Archive -Path $vmZip -DestinationPath $vmDir -Force
    $vmSetup = Get-ChildItem $vmDir -Filter "*.exe" | Select-Object -First 1
    if (-not $vmSetup) { throw "VoiceMeeter 安装程序不完整" }

    Set-Result "running" "请完成 VoiceMeeter 安装…"
    $p = Start-Process -FilePath $vmSetup.FullName -Wait -PassThru
    if ($p.ExitCode -notin @(0, 3010)) { throw "VoiceMeeter 安装未完成（$($p.ExitCode)）" }

    Set-Result "running" "请完成 ASIO4ALL 安装…"
    $p = Start-Process -FilePath $asioExe -Wait -PassThru
    if ($p.ExitCode -notin @(0, 3010)) { throw "ASIO4ALL 安装未完成（$($p.ExitCode)）" }

    Set-Result "restart" "桥接组件已安装，请重启电脑后再次点击启用桥接"
}
catch {
    Set-Result "error" $_.Exception.Message
    exit 1
}
