param([string]$BuildDirectory = "build-midi-input")
$ErrorActionPreference = 'Stop'
$Evidence = Join-Path $BuildDirectory 'evidence'
New-Item -ItemType Directory $Evidence -Force | Out-Null
$Baseline = Join-Path (Resolve-Path $BuildDirectory) 'previous-test.cpp'
# The preceding release candidate failed before its first test output. Keep
# that exact source as the counterexample, with the same production runtime.
$Previous = git show 'HEAD^:tests/test_midi_onset.cpp'
if($LASTEXITCODE -ne 0){throw 'Cannot read the exact preceding test fixture'}
[IO.File]::WriteAllLines($Baseline, $Previous, (New-Object Text.UTF8Encoding($false)))
cmake -S tools/midi-input-check -B $BuildDirectory -G 'Visual Studio 17 2022' -A x64 "-DBASELINE_SOURCE=$Baseline"
if($LASTEXITCODE){throw 'MIDI regression configure failed'}
cmake --build $BuildDirectory --config Release --parallel 4
if($LASTEXITCODE){throw 'MIDI regression compile failed'}

function Invoke-TestBinary([string]$Name) {
    $Exe = (Resolve-Path (Join-Path $BuildDirectory "Release/$Name.exe")).Path
    $Bytes = [IO.File]::ReadAllBytes($Exe)
    $Pe = [BitConverter]::ToInt32($Bytes, 0x3c)
    $StackReserve = [BitConverter]::ToUInt64($Bytes, $Pe + 24 + 72)
    if($StackReserve -ne 1048576){throw "$Name changed the default 1 MiB Windows stack"}
    $Process = New-Object Diagnostics.Process
    $Process.StartInfo.FileName = $Exe
    $Process.StartInfo.UseShellExecute = $false
    $Process.StartInfo.CreateNoWindow = $true
    $Process.StartInfo.RedirectStandardOutput = $true
    $Process.StartInfo.RedirectStandardError = $true
    if(!$Process.Start()){throw "Could not start $Name"}
    if(!$Process.WaitForExit(60000)){$Process.Kill();throw "$Name exceeded 60 seconds"}
    $Process.StandardOutput.ReadToEnd() | Set-Content (Join-Path $Evidence "$Name-out.txt") -Encoding utf8
    $Process.StandardError.ReadToEnd() | Set-Content (Join-Path $Evidence "$Name-err.txt") -Encoding utf8
    [pscustomobject]@{name=$Name;exitCode=$Process.ExitCode;stackReserve=$StackReserve}
}
$Before = Invoke-TestBinary 'OnsetBaseline'
$After = Invoke-TestBinary 'OnsetFixed'
@($Before,$After) | ConvertTo-Json | Set-Content (Join-Path $Evidence 'stack-comparison.json') -Encoding utf8
# 0xc00000fd is Windows' actual STATUS_STACK_OVERFLOW, not a generic failure.
if($Before.exitCode -ne -1073741571){throw "Expected the previous fixture to reproduce stack overflow, got $($Before.exitCode)"}
if($After.exitCode -ne 0){throw "Production MIDI regression failed: $($After.exitCode)"}
ctest --test-dir $BuildDirectory -C Release --output-on-failure
if($LASTEXITCODE){throw 'Production MIDI regression CTest failed'}
Write-Host 'Exact old fixture: STATUS_STACK_OVERFLOW. Scoped heap fixture: all production MIDI assertions passed at unchanged default stack size.'
