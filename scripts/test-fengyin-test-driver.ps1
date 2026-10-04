param(
    [Parameter(Mandatory = $true)]
    [string]$DriverDir
)

$ErrorActionPreference = "Stop"
$DriverDir = [System.IO.Path]::GetFullPath($DriverDir)
$Required = @(
    "FengYinAudio.inf",
    "FengYinAudio.cat",
    "TabletAudioSample.sys",
    "TabletAudioSample.cer"
)
foreach ($Name in $Required) {
    if (-not (Test-Path (Join-Path $DriverDir $Name) -PathType Leaf)) {
        throw "测试驱动包缺少：$Name"
    }
}

$Certificate = [System.Security.Cryptography.X509Certificates.X509Certificate2]::new(
    (Join-Path $DriverDir "TabletAudioSample.cer")
)
if ($Certificate.Subject -notmatch "WDKTestCert|Windows Test|Test Certificate") {
    throw "证书不是 WDK 测试证书：$($Certificate.Subject)"
}

foreach ($Name in @("FengYinAudio.cat", "TabletAudioSample.sys")) {
    $Signature = Get-AuthenticodeSignature (Join-Path $DriverDir $Name)
    if (-not $Signature.SignerCertificate) {
        throw "$Name 没有 Authenticode 签名。"
    }
    if ($Signature.SignerCertificate.Thumbprint -ne $Certificate.Thumbprint) {
        throw "$Name 与随包测试证书不匹配。"
    }
    if ($Signature.Status -notin @("Valid", "UnknownError")) {
        # A clean machine may not trust the test root until installation time;
        # UnknownError is acceptable only when signer identity still matches.
        throw "$Name 签名状态异常：$($Signature.Status)"
    }
}

$Inf = Get-Content (Join-Path $DriverDir "FengYinAudio.inf") -Raw
foreach ($Needle in @("Root\FengYinAudioEngine", "FengYinAudio.cat", "TabletAudioSample.sys")) {
    if ($Inf -notlike "*$Needle*") { throw "INF 缺少关键声明：$Needle" }
}

Write-Host "测试驱动验证通过：SYS、CAT 与随包 WDK 测试证书一致。" -ForegroundColor Green
