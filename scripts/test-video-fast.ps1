param(
    # Requires the generated red video/nonzero audio diagnostic fixture.
    [Parameter(Mandatory=$true)][string]$Fixture,
    [string]$BuildDir = "build-video-fast",
    [string]$JuceSource = "",
    [ValidateRange(1,16)][int]$Jobs = 4,
    [switch]$BuildOnly
)

# Development-only: reuses the local build tree; never packages or installs.
$ErrorActionPreference = "Stop"
$env:PYTHONUTF8 = "1"
$Root = Split-Path -Parent $PSScriptRoot
if (-not [IO.Path]::IsPathRooted($BuildDir)) { $BuildDir = Join-Path $Root $BuildDir }
$Fixture = (Resolve-Path -LiteralPath $Fixture).Path
$Vlc = Join-Path $Root "third_party/vlc"
$Nuget = Join-Path $Root "third_party/nuget-packages"
foreach ($Relative in @("libvlc.dll", "libvlccore.dll", "sdk/include/vlc/vlc.h", "plugins/codec/libavcodec_plugin.dll", "fengyin-source.json")) {
    if (-not (Test-Path -LiteralPath (Join-Path $Vlc $Relative))) {
        throw "Prepare the pinned VLC dependencies first with scripts/prepare_vlc.py; missing $Relative"
    }
}
$Marker = Get-Content -LiteralPath (Join-Path $Vlc "fengyin-source.json") -Raw | ConvertFrom-Json
if ($Marker.version -ne "3.0.24" -or $Marker.sha256 -ne "3da3f37ee2b2b10e5c788d830a3b513658c36521ea1b0f966ed86eb69f41ef3c") {
    throw "VLC dependency marker differs from the pinned diagnostic runtime"
}
if (-not (Test-Path -LiteralPath (Join-Path $Nuget "Microsoft.Web.WebView2.1.0.3485.44"))) {
    throw "Prepare Microsoft.Web.WebView2 1.0.3485.44 in third_party/nuget-packages first"
}
$Configure = @("-S", (Join-Path $Root "tools/vlc-webview-check"), "-B", $BuildDir,
    "-G", "Visual Studio 17 2022", "-A", "x64", "-DJUCE_WEBVIEW2_PACKAGE_LOCATION=$Nuget")
if ($JuceSource) { $Configure += "-DFETCHCONTENT_SOURCE_DIR_JUCE=$((Resolve-Path -LiteralPath $JuceSource).Path)" }
& cmake @Configure
if ($LASTEXITCODE -ne 0) { throw "Diagnostic configure failed: $LASTEXITCODE" }
& cmake --build $BuildDir --config Release --target FengYinVlcWebViewCheck --parallel $Jobs
if ($LASTEXITCODE -ne 0) { throw "Diagnostic build failed: $LASTEXITCODE" }

# Copy only runtime files, not source archives/SDK; robocopy skips unchanged files.
$Runtime = Join-Path $BuildDir "Release/components/VLC"
& robocopy $Vlc $Runtime /E /XD sdk source /R:1 /W:1 /NFL /NDL /NJH /NJS
if ($LASTEXITCODE -ge 8) { throw "VLC runtime copy failed: $LASTEXITCODE" }
if ($BuildOnly) {
    Write-Host "Build only; no tests passed or installer produced. $BuildDir"
    exit 0
}
& python (Join-Path $PSScriptRoot "check_vlc_webview.py") $BuildDir $Fixture
if ($LASTEXITCODE -ne 0) { throw "Video diagnostic failed; evidence: $BuildDir/evidence" }
Write-Host "Video/window diagnostic passed only. Real instrument, speaker and recording acceptance remains required."
