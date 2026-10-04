param(
    [Parameter(Mandatory = $true)]
    [string]$BuildOutputDir,
    [string]$OutputDirectory = "",
    [string]$CertificateThumbprint = "",
    [string]$TimestampUrl = "http://timestamp.digicert.com"
)

$ErrorActionPreference = "Stop"
$ProjectRoot = Split-Path -Parent $PSScriptRoot
if ([string]::IsNullOrWhiteSpace($OutputDirectory)) {
    $OutputDirectory = Join-Path $ProjectRoot "dist\driver-submission"
}
$BuildOutputDir = [System.IO.Path]::GetFullPath($BuildOutputDir)
$OutputDirectory = [System.IO.Path]::GetFullPath($OutputDirectory)
$PackageName = "FengYinAudio"
$StageDir = Join-Path $OutputDirectory $PackageName

foreach ($Name in @("FengYinAudio.inf", "FengYinAudio.cat", "TabletAudioSample.sys", "TabletAudioSample.pdb")) {
    if (-not (Test-Path (Join-Path $BuildOutputDir $Name) -PathType Leaf)) {
        throw "驱动提交材料不完整，缺少：$Name"
    }
}

if (Test-Path $OutputDirectory) { Remove-Item $OutputDirectory -Recurse -Force }
New-Item -ItemType Directory -Force -Path $StageDir | Out-Null
foreach ($Name in @("FengYinAudio.inf", "FengYinAudio.cat", "TabletAudioSample.sys", "TabletAudioSample.pdb")) {
    Copy-Item (Join-Path $BuildOutputDir $Name) (Join-Path $StageDir $Name) -Force
}

$DdfPath = Join-Path $OutputDirectory "FengYinAudio.ddf"
$CabPath = Join-Path $OutputDirectory "Disk1\FengYinAudio-attestation.cab"
$Ddf = @"
.OPTION EXPLICIT
.Set CabinetFileCountThreshold=0
.Set FolderFileCountThreshold=0
.Set FolderSizeThreshold=0
.Set MaxCabinetSize=0
.Set MaxDiskFileCount=0
.Set MaxDiskSize=0
.Set CompressionType=MSZIP
.Set Cabinet=on
.Set Compress=on
.Set CabinetNameTemplate=FengYinAudio-attestation.cab
.Set DiskDirectoryTemplate=Disk1
.Set DestinationDir=$PackageName
"$StageDir\FengYinAudio.inf"
"$StageDir\FengYinAudio.cat"
"$StageDir\TabletAudioSample.sys"
"$StageDir\TabletAudioSample.pdb"
"@
Set-Content -Path $DdfPath -Value $Ddf -Encoding ASCII

$MakeCab = Get-Command makecab.exe -ErrorAction SilentlyContinue
if (-not $MakeCab) { throw "未找到 Windows makecab.exe。" }
Push-Location $OutputDirectory
try {
    & $MakeCab.Source /F $DdfPath | Out-Host
    if ($LASTEXITCODE -ne 0 -or -not (Test-Path $CabPath)) {
        throw "生成 Partner Center 提交 CAB 失败。"
    }
} finally {
    Pop-Location
}

if (-not [string]::IsNullOrWhiteSpace($CertificateThumbprint)) {
    $SignTool = Get-Command signtool.exe -ErrorAction SilentlyContinue
    if (-not $SignTool) { throw "未找到 signtool.exe，无法使用 EV 证书签署提交 CAB。" }
    & $SignTool.Source sign /sha1 $CertificateThumbprint /fd sha256 /tr $TimestampUrl /td sha256 /v $CabPath
    if ($LASTEXITCODE -ne 0) { throw "EV 证书签署提交 CAB 失败。" }
    & $SignTool.Source verify /pa /v $CabPath | Out-Null
    if ($LASTEXITCODE -ne 0) { throw "提交 CAB 的 EV 签名验证失败。" }
}

Write-Host "Partner Center 提交包已生成：$CabPath" -ForegroundColor Green
if ([string]::IsNullOrWhiteSpace($CertificateThumbprint)) {
    Write-Host "下一步：用已登记到 Partner Center 的 EV/Authenticode 证书签署此 CAB，再上传到 Hardware Dashboard。" -ForegroundColor Yellow
}

