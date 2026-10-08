$ErrorActionPreference = 'Stop'
$installer = Join-Path $PSScriptRoot '../third_party/fresh-air/Setup Fresh Air v1.0.8.exe'
$report = Join-Path (Get-Location) 'fresh-air-install-test'
New-Item -ItemType Directory -Force -Path $report | Out-Null
$log = Join-Path $report 'install.log'
$inf = Join-Path $report 'install.inf'
$process = Start-Process -FilePath $installer -ArgumentList "/VERYSILENT /SUPPRESSMSGBOXES /NORESTART /SP- /TYPE=full /LOG=`"$log`" /SAVEINF=`"$inf`"" -PassThru
if (-not $process.WaitForExit(180000)) {
    Stop-Process -Id $process.Id -Force
    throw 'Fresh Air silent installation exceeded 180 seconds. See installer logs.'
}
"Installer exit code: $($process.ExitCode)" | Out-File (Join-Path $report 'result.txt')
$root = Join-Path $env:CommonProgramFiles 'VST3'
$paths = @(
    (Join-Path $root 'Fresh Air.vst3'),
    (Join-Path $root 'Slate Digital/Fresh Air.vst3'),
    (Join-Path $root 'Fresh Air.vst3/Contents/x86_64-win/Fresh Air.vst3'),
    (Join-Path $root 'Slate Digital/Fresh Air.vst3/Contents/x86_64-win/Fresh Air.vst3')
)
$found = @($paths | Where-Object { Test-Path -LiteralPath $_ -PathType Leaf })
Get-ChildItem -LiteralPath $root -Recurse -ErrorAction SilentlyContinue |
    Select-Object FullName, Length | Out-File (Join-Path $report 'vst3-files.txt')
if ($found.Count -eq 0) {
    throw 'Fresh Air did not install a discoverable x64 VST3 on the clean Windows runner.'
}
$found | Out-File (Join-Path $report 'result.txt') -Append
Write-Host "Verified Fresh Air VST3: $($found -join ', ')"
