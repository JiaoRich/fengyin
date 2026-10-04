param(
    [Parameter(Mandatory = $true)]
    [string]$DriverDir,
    [switch]$Quiet
)

$ErrorActionPreference = "Stop"
$ExpectedFiles = @("FengYinAudio.inf", "FengYinAudio.cat", "TabletAudioSample.sys")

function Find-SignTool {
    $FromPath = Get-Command signtool.exe -ErrorAction SilentlyContinue
    if ($FromPath) { return $FromPath.Source }

    $KitsRoot = Join-Path ${env:ProgramFiles(x86)} "Windows Kits\10\bin"
    if (-not (Test-Path $KitsRoot)) { return $null }
    return Get-ChildItem $KitsRoot -Filter signtool.exe -File -Recurse -ErrorAction SilentlyContinue |
        Where-Object { $_.FullName -match '[\\/]x64[\\/]signtool\.exe$' } |
        Sort-Object FullName -Descending |
        Select-Object -First 1 -ExpandProperty FullName
}

$ResolvedDriverDir = [System.IO.Path]::GetFullPath($DriverDir)
if (-not (Test-Path $ResolvedDriverDir -PathType Container)) {
    throw "正式驱动目录不存在：$ResolvedDriverDir"
}

foreach ($Name in $ExpectedFiles) {
    if (-not (Test-Path (Join-Path $ResolvedDriverDir $Name) -PathType Leaf)) {
        throw "正式驱动包不完整，缺少：$Name"
    }
}

$InfPath = Join-Path $ResolvedDriverDir "FengYinAudio.inf"
$InfText = Get-Content $InfPath -Raw
foreach ($RequiredText in @("Root\FengYinAudioEngine", "FengYinAudio.cat", "TabletAudioSample.sys")) {
    if ($InfText -notmatch [regex]::Escape($RequiredText)) {
        throw "正式驱动 INF 与风吟固定设备不匹配，缺少：$RequiredText"
    }
}

foreach ($Name in @("FengYinAudio.cat", "TabletAudioSample.sys")) {
    $Path = Join-Path $ResolvedDriverDir $Name
    $Signature = Get-AuthenticodeSignature $Path
    if ($Signature.Status -ne "Valid" -or -not $Signature.SignerCertificate) {
        throw "正式驱动签名无效：$Name（$($Signature.Status)）"
    }
    $Subject = $Signature.SignerCertificate.Subject
    if ($Subject -match "WDKTestCert|Windows Test|Test Certificate") {
        throw "禁止导入测试签名驱动：$Name（$Subject）"
    }
    if ($Subject -notmatch "Microsoft Windows Hardware Compatibility") {
        throw "驱动不是 Microsoft Hardware Dev Center 返回的正式签名：$Name（$Subject）"
    }
}

$SignTool = Find-SignTool
if (-not $SignTool) {
    throw "未找到 Windows SDK signtool.exe，无法核对驱动目录签名与文件哈希。"
}

& $SignTool verify /kp /v (Join-Path $ResolvedDriverDir "FengYinAudio.cat") | Out-Null
if ($LASTEXITCODE -ne 0) { throw "FengYinAudio.cat 内核签名验证失败。" }
& $SignTool verify /kp /v /c (Join-Path $ResolvedDriverDir "FengYinAudio.cat") `
    (Join-Path $ResolvedDriverDir "TabletAudioSample.sys") | Out-Null
if ($LASTEXITCODE -ne 0) { throw "TabletAudioSample.sys 与正式目录文件不匹配。" }

if (-not $Quiet) {
    Write-Host "正式驱动验证通过：微软签名、固定硬件 ID、目录和 SYS 哈希均有效。" -ForegroundColor Green
}

