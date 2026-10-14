$ErrorActionPreference = 'Stop'
$source = Join-Path $PSScriptRoot '../assets/components/magic.CURVE-1.0.2-Windows-Setup.exe'
$expected = '5AEBF446018D5F8FCE87F332E9D8A01194A98E02536DE132BF1C64939417BDBD'
if ((Get-FileHash $source -Algorithm SHA256).Hash -ne $expected) { throw 'magic.CURVE installer hash mismatch' }
$report = Join-Path (Get-Location) 'magic-curve-install-test'
New-Item -ItemType Directory -Force -Path $report | Out-Null
$log = Join-Path $report 'install.log'
$process = Start-Process -FilePath $source -ArgumentList "/VERYSILENT /SUPPRESSMSGBOXES /NORESTART /SP- /LOG=`"$log`"" -PassThru
if (-not $process.WaitForExit(180000)) { throw 'magic.CURVE installer did not finish within 180 seconds' }
if ($process.ExitCode -ne 0 -and $process.ExitCode -ne 3010) { throw "magic.CURVE installer failed: $($process.ExitCode)" }
$paths = @(Get-ChildItem (Join-Path $env:CommonProgramFiles 'VST3') -Recurse -File -ErrorAction SilentlyContinue | Where-Object { $_.Name -eq 'magic.CURVE.vst3' })
if ($paths.Count -eq 0) { throw 'magic.CURVE x64 VST3 binary not found after installation' }
$paths.FullName | Out-File (Join-Path $report 'installed-paths.txt')
foreach ($path in $paths) { Get-FileHash $path.FullName -Algorithm SHA256 | Out-File (Join-Path $report 'installed-hashes.txt') -Append }
"Exit: $($process.ExitCode)" | Out-File (Join-Path $report 'result.txt')
