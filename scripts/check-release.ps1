param(
    [Parameter(Mandatory = $true)]
    [string]$PackageDir
)

$ErrorActionPreference = "Stop"
$RequiredFiles = @(
    "FengYin.exe",
    "FengYinAudioEngine.exe",
    "FengYinAudioWatchdog.exe",
    "README.md",
    "THIRD_PARTY_NOTICES.md",
    "MicrosoftEdgeWebView2RuntimeInstallerX64.exe",
    "components\ASIO4ALL\ASIO4ALL_2_22.exe",
    "components\VB-CABLE\VBCABLE_Setup_x64.exe",
    "components\Fresh Air\Setup Fresh Air v1.0.8.exe"
)
foreach ($Name in $RequiredFiles) {
    $Path = Join-Path $PackageDir $Name
    if (-not (Test-Path $Path)) { throw "发布检查失败，缺少：$Name" }
}

$Forbidden = Get-ChildItem $PackageDir -Recurse -File | Where-Object {
    $_.Name -match "private-key|license-private|\.pfx$|\.pem$"
}
if ($Forbidden) {
    throw "发布检查失败：客户目录中发现疑似私钥文件：$($Forbidden.FullName -join ', ')"
}

$DriverDir = Join-Path $PackageDir "driver"
if (Test-Path $DriverDir) {
    foreach ($Name in @("FengYinAudio.inf", "FengYinAudio.cat", "TabletAudioSample.sys")) {
        if (-not (Test-Path (Join-Path $DriverDir $Name))) {
            throw "发布检查失败，驱动包缺少：$Name"
        }
    }
    & (Join-Path $PSScriptRoot "test-fengyin-production-driver.ps1") -DriverDir $DriverDir -Quiet
    if ($LASTEXITCODE -ne 0) { throw "发布检查失败：正式驱动验证脚本返回 $LASTEXITCODE" }
}

$Size = (Get-ChildItem $PackageDir -Recurse -File | Measure-Object Length -Sum).Sum
if ($Size -lt 1MB) { throw "发布检查失败：客户程序目录体积异常。" }

$FreshAirInstaller = Join-Path $PackageDir "components\Fresh Air\Setup Fresh Air v1.0.8.exe"
$FreshAirHash = (Get-FileHash $FreshAirInstaller -Algorithm SHA256).Hash
if ($FreshAirHash -ne "C50D3CE92ACB7524B1A4C962F9ADFCCE100FBEB957715B8788051D73F0D02A73") {
    throw "发布检查失败：Fresh Air 1.0.8 安装程序哈希不匹配。"
}
$WebViewInstaller = Join-Path $PackageDir "MicrosoftEdgeWebView2RuntimeInstallerX64.exe"
if ((Get-Item $WebViewInstaller).Length -lt 100MB) {
    throw "发布检查失败：WebView2 x64 离线运行环境文件不完整。"
}
$WebViewSignature = Get-AuthenticodeSignature -FilePath $WebViewInstaller
if ($WebViewSignature.Status -ne 'Valid' -or
    $WebViewSignature.SignerCertificate.Subject -notmatch 'Microsoft Corporation') {
    throw "发布检查失败：WebView2 x64 离线运行环境未通过微软数字签名验证。"
}

Write-Host "发布检查通过：主程序、音频组件、Fresh Air 1.0.8 和说明文件齐全，客户目录未发现私钥。" -ForegroundColor Green
