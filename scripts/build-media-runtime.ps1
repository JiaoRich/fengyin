param([string]$BuildDir = 'build-media-runtime', [string]$JuceSource = '')
$ErrorActionPreference = 'Stop'
$env:PYTHONUTF8 = '1'
$Root = Split-Path -Parent $PSScriptRoot
if (-not [IO.Path]::IsPathRooted($BuildDir)) { $BuildDir = Join-Path $Root $BuildDir }
function Checked([string]$Label,[scriptblock]$Action) {
    & $Action
    if ($LASTEXITCODE -ne 0) { throw "$Label failed: $LASTEXITCODE" }
}
Checked 'Source regressions' { python -B -m unittest discover -s (Join-Path $Root 'tests') -p 'test_*.py' }
$Vlc = Join-Path $Root 'third_party/vlc'
$Audio = Join-Path $Root 'third_party/recording-audio'
$Nuget = Join-Path $Root 'third_party/nuget-packages'
if (-not (Test-Path -LiteralPath (Join-Path $Vlc 'fengyin-source.json'))) { throw 'Prepare pinned VLC dependencies first' }
Checked 'Audio-only source verification' { python (Join-Path $PSScriptRoot 'prepare_audio_recording.py') $Audio }
$ConfigureArgs = @('-S',$Root,'-B',$BuildDir,'-G','Visual Studio 17 2022','-A','x64',
    '-DBUILD_TESTING=OFF','-DFENGYIN_ARCHIVED_VIDEO_RECORDING=OFF',
    "-DJUCE_WEBVIEW2_PACKAGE_LOCATION=$Nuget","-DFENGYIN_VLC_ROOT=$Vlc","-DFENGYIN_RECORDING_ROOT=$Audio")
if ($JuceSource) {
    $JuceSource = (Resolve-Path -LiteralPath $JuceSource).Path
    Checked 'Production JUCE patches' { cmake "-DJUCE_SOURCE_DIR=$JuceSource" -P (Join-Path $PSScriptRoot 'patch-juce-wasapi-raw.cmake') }
    $ConfigureArgs += "-DFETCHCONTENT_SOURCE_DIR_JUCE=$JuceSource"
}
Checked 'Configure development runtime' { cmake @ConfigureArgs }
Checked 'Build runtime targets only' {
    cmake --build $BuildDir --config Release --target FengYin FengYinRecorder FengYinAudioEngine FengYinAudioWatchdog FengYinDriverSetup FengYinScreenCapture --parallel 4
}
$Revision = (& git -C $Root rev-parse --short=12 HEAD).Trim()
if ($LASTEXITCODE -ne 0) { throw 'Cannot identify runtime source revision' }
$Package = Join-Path $Root ("dist/media-runtime-$Revision")
if (Test-Path -LiteralPath $Package) { throw 'Use a new output directory; do not mix runtime revisions' }
New-Item -ItemType Directory -Path $Package -Force | Out-Null
$Main = Join-Path $BuildDir 'FengYin_artefacts/Release/FengYin.exe'
Copy-Item -LiteralPath $Main -Destination (Join-Path $Package 'FengYin.exe')
foreach ($Name in @('FengYinRecorder','FengYinAudioEngine','FengYinAudioWatchdog','FengYinDriverSetup','FengYinScreenCapture')) {
    Copy-Item -LiteralPath (Join-Path $BuildDir "Release/$Name.exe") -Destination $Package
}
New-Item -ItemType Directory -Path (Join-Path $Package 'components') -Force | Out-Null
Copy-Item -LiteralPath $Vlc -Destination (Join-Path $Package 'components/VLC') -Recurse
Copy-Item -LiteralPath $Audio -Destination (Join-Path $Package 'components/Recording') -Recurse
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'media-runtime-test') -Destination $Package -Recurse
Copy-Item -LiteralPath (Join-Path $Root 'THIRD_PARTY_NOTICES.md') -Destination $Package

# This is an actual audio-only encoder check, not target-device capture acceptance.
$Recorder = Join-Path $Package 'FengYinRecorder.exe'
$Wav = Join-Path $BuildDir ('runtime-selftest-' + [guid]::NewGuid().ToString('N') + '.wav')
$Process = Start-Process -FilePath $Recorder -ArgumentList @('--audio-selftest',('"{0}"' -f $Wav)) -PassThru
if (-not $Process.WaitForExit(60000)) { throw 'Recorder selftest timed out; no test package can pass' }
$Process.Refresh()
if ($Process.ExitCode -ne 0) { throw "Recorder selftest failed: $($Process.ExitCode)" }
Checked 'Verify actual WAV samples' { python (Join-Path $PSScriptRoot 'verify_system_recording.py') $Wav }
$Files = @(Get-ChildItem -LiteralPath $Package -File -Recurse | ForEach-Object {
    [pscustomobject]@{Path=$_.FullName.Substring($Package.Length+1);Bytes=$_.Length;SHA256=(Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash}
})
[pscustomobject]@{Revision=$Revision;Commit=(& git -C $Root rev-parse HEAD).Trim();Kind='development runtime, not installer';
    Pending='Full Windows CTest, target instrument/video/recording acceptance, installation and upgrade';Files=$Files} |
    ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $Package 'build.json') -Encoding UTF8
Write-Host "Development runtime ready: $Package. Not a release acceptance result."
