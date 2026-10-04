param(
    [Parameter(Mandatory = $true)]
    [string]$SignedPackage,
    [string]$Destination = ""
)

$ErrorActionPreference = "Stop"
$ProjectRoot = Split-Path -Parent $PSScriptRoot
if ([string]::IsNullOrWhiteSpace($Destination)) {
    $Destination = Join-Path $ProjectRoot "third_party\fengyin-audio-driver\windows-x64"
}
$SignedPackage = [System.IO.Path]::GetFullPath($SignedPackage)
$Destination = [System.IO.Path]::GetFullPath($Destination)
$Scratch = Join-Path $env:TEMP ("fengyin-signed-driver-" + [guid]::NewGuid().ToString("N"))

try {
    New-Item -ItemType Directory -Force -Path $Scratch | Out-Null
    if (Test-Path $SignedPackage -PathType Container) {
        Copy-Item (Join-Path $SignedPackage "*") $Scratch -Recurse -Force
    } elseif ([System.IO.Path]::GetExtension($SignedPackage) -ieq ".zip") {
        Expand-Archive -Path $SignedPackage -DestinationPath $Scratch -Force
    } else {
        throw "微软签名返回包必须是目录或 ZIP：$SignedPackage"
    }

    $Resolved = @{}
    foreach ($Name in @("FengYinAudio.inf", "FengYinAudio.cat", "TabletAudioSample.sys")) {
        $Matches = @(Get-ChildItem $Scratch -Recurse -File -Filter $Name)
        if ($Matches.Count -ne 1) {
            throw "签名返回包中应且只能包含一个 $Name，实际找到 $($Matches.Count) 个。"
        }
        $Resolved[$Name] = $Matches[0].FullName
    }

    $VerifyDir = Join-Path $Scratch "verify"
    New-Item -ItemType Directory -Force -Path $VerifyDir | Out-Null
    foreach ($Name in $Resolved.Keys) { Copy-Item $Resolved[$Name] (Join-Path $VerifyDir $Name) -Force }
    & (Join-Path $PSScriptRoot "test-fengyin-production-driver.ps1") -DriverDir $VerifyDir -Quiet

    $DestinationParent = Split-Path -Parent $Destination
    New-Item -ItemType Directory -Force -Path $DestinationParent | Out-Null
    $StagedDestination = "$Destination.new"
    if (Test-Path $StagedDestination) { Remove-Item $StagedDestination -Recurse -Force }
    New-Item -ItemType Directory -Force -Path $StagedDestination | Out-Null
    foreach ($Name in $Resolved.Keys) { Copy-Item $Resolved[$Name] (Join-Path $StagedDestination $Name) -Force }
    & (Join-Path $PSScriptRoot "test-fengyin-production-driver.ps1") -DriverDir $StagedDestination -Quiet

    if (Test-Path $Destination) { Remove-Item $Destination -Recurse -Force }
    Move-Item $StagedDestination $Destination
    Write-Host "微软正式签名驱动已安全导入：$Destination" -ForegroundColor Green
} finally {
    if (Test-Path $Scratch) { Remove-Item $Scratch -Recurse -Force }
}

