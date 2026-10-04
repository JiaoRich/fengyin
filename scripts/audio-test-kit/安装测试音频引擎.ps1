$ErrorActionPreference = "Stop"
$KitRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$DriverDir = Join-Path $KitRoot "test-driver"
$Setup = Join-Path $KitRoot "FengYinDriverSetup.exe"
$CertificatePath = Join-Path $DriverDir "TabletAudioSample.cer"
$InfPath = Join-Path $DriverDir "FengYinAudio.inf"

function Assert-Administrator {
    $Identity = [Security.Principal.WindowsIdentity]::GetCurrent()
    $Principal = [Security.Principal.WindowsPrincipal]::new($Identity)
    if (-not $Principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
        Start-Process powershell.exe -Verb RunAs -ArgumentList @(
            "-NoProfile", "-ExecutionPolicy", "Bypass", "-File", "`"$PSCommandPath`""
        )
        exit
    }
}

Assert-Administrator
foreach ($Path in @($Setup, $CertificatePath, $InfPath)) {
    if (-not (Test-Path $Path -PathType Leaf)) { throw "测试包不完整，缺少：$Path" }
}

$StartOptions = (Get-ItemProperty "HKLM:\SYSTEM\CurrentControlSet\Control" -Name SystemStartOptions -ErrorAction SilentlyContinue).SystemStartOptions
$BcdState = (& bcdedit.exe /enum "{current}" 2>$null) -join "`n"
$TestSigningEnabled = $StartOptions -match "TESTSIGNING" -or $BcdState -match "(?im)^testsigning\s+(Yes|是|On)\s*$"
if (-not $TestSigningEnabled) {
    throw "Windows 尚未运行在测试签名模式。请先运行‘开启Windows测试模式.ps1’，重启后再安装。"
}

$Certificate = [System.Security.Cryptography.X509Certificates.X509Certificate2]::new($CertificatePath)
if ($Certificate.Subject -notmatch "WDKTestCert|Windows Test|Test Certificate") {
    throw "随包证书不是预期的 WDK 测试证书，已停止安装。"
}
$Thumbprint = $Certificate.Thumbprint
$RootAlreadyPresent = Test-Path "Cert:\LocalMachine\Root\$Thumbprint"
$PublisherAlreadyPresent = Test-Path "Cert:\LocalMachine\TrustedPublisher\$Thumbprint"

try {
    if (-not $RootAlreadyPresent) {
        Import-Certificate -FilePath $CertificatePath -CertStoreLocation "Cert:\LocalMachine\Root" | Out-Null
    }
    if (-not $PublisherAlreadyPresent) {
        Import-Certificate -FilePath $CertificatePath -CertStoreLocation "Cert:\LocalMachine\TrustedPublisher" | Out-Null
    }

    foreach ($Name in @("FengYinAudio.cat", "TabletAudioSample.sys")) {
        $Signature = Get-AuthenticodeSignature (Join-Path $DriverDir $Name)
        if ($Signature.Status -ne "Valid" -or $Signature.SignerCertificate.Thumbprint -ne $Thumbprint) {
            throw "$Name 的测试签名无效或与随包证书不一致。"
        }
    }

    & $Setup --install $InfPath
    $InstallResult = $LASTEXITCODE
    if ($InstallResult -ne 0 -and $InstallResult -ne 3010) {
        throw "风吟测试音频驱动安装失败（错误码 $InstallResult）。"
    }
    & $Setup --check
    if ($LASTEXITCODE -ne 0) { throw "安装结束后没有检测到风吟虚拟音频设备。" }

    $StateDir = Join-Path $env:ProgramData "FengYin"
    New-Item -ItemType Directory -Force -Path $StateDir | Out-Null
    Set-Content (Join-Path $StateDir "test-driver-cert-thumbprint.txt") $Thumbprint -Encoding Ascii
    Write-Host "测试音频引擎安装成功。现在直接运行本目录中的 FengYin.exe 开始实机测试。" -ForegroundColor Green
    if ($InstallResult -eq 3010) { Write-Host "Windows 要求重启后驱动才会生效。" -ForegroundColor Yellow }
} catch {
    if (-not $PublisherAlreadyPresent) { Remove-Item "Cert:\LocalMachine\TrustedPublisher\$Thumbprint" -Force -ErrorAction SilentlyContinue }
    if (-not $RootAlreadyPresent) { Remove-Item "Cert:\LocalMachine\Root\$Thumbprint" -Force -ErrorAction SilentlyContinue }
    throw
}

Read-Host "按回车退出"
