$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
. (Join-Path $root 'installer\VCRuntimePolicy.ps1')
$required=[version]'14.51.36247.0'
$cases=@(
    @($null,$false,'install'),@([version]'14.44.35211.0',$true,'install'),
    @($required,$true,'skip'),@($required,$false,'repair'),
    @([version]'14.60.1.0',$true,'skip'),@([version]'14.60.1.0',$false,'newer-needs-repair')
)
foreach($case in $cases){if((Get-VCRuntimeAction $required $case[0] $case[1])-ne $case[2]){throw ('Wrong runtime action: '+$case[2])}}
$exits=@(@(0,$true,$true,'complete'),@(3010,$true,$true,'restart'),@(3010,$false,$true,'restart-before-retry'),
 @(0,$false,$true,'failed'),@(1602,$true,$true,'failed'),@(1618,$false,$false,'failed'),
 @(1638,$true,$true,'complete'),@(-2147023258,$true,$true,'complete'),@(1638,$false,$false,'failed'))
foreach($case in $exits){if((Get-VCRuntimeExitAction $case[0] $case[1] $case[2])-ne $case[3]){throw ('Wrong runtime exit action: '+$case[3])}}
$evidence=Join-Path $root 'vc-runtime-test'
New-Item -ItemType Directory -Force -Path $evidence | Out-Null
$package=Join-Path $root 'third_party\vc-runtime'
& (Join-Path $PSScriptRoot 'prepare-vc-runtime.ps1') -DestinationDirectory $package
if($LASTEXITCODE -ne 0){throw 'Runtime prepare failed'}
$ps=Join-Path $env:SystemRoot 'System32\WindowsPowerShell\v1.0\powershell.exe'
# Exercise the real signed package + registry/system-loader path. CI may already
# have a runtime: evidence explicitly does not call this a clean-machine test.
& $ps -NoProfile -NonInteractive -ExecutionPolicy Bypass -File (Join-Path $package 'Install-VCRuntime.ps1') -PackageDirectory $package -LogDirectory $evidence
if($LASTEXITCODE -notin @(0,3010)){throw ('Actual runtime install/verify failed: '+$LASTEXITCODE)}
& $ps -NoProfile -NonInteractive -ExecutionPolicy Bypass -File (Join-Path $package 'Install-VCRuntime.ps1') -PackageDirectory $package -LogDirectory $evidence
if($LASTEXITCODE -ne 0){throw 'Repeated healthy runtime preparation should skip with success'}
$tampered=Join-Path $env:TEMP ('fy-vc-tamper-'+[guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory $tampered | Out-Null
try {
    Copy-Item (Join-Path $package '*') $tampered -Force
    [IO.File]::WriteAllBytes((Join-Path $tampered 'VC_redist.x64.exe'),[byte[]]@(77,90,0))
    $arguments='-NoProfile -NonInteractive -ExecutionPolicy Bypass -File "'+(Join-Path $tampered 'Install-VCRuntime.ps1')+'" -PackageDirectory "'+$tampered+'" -VerifyOnly'
    $test=Start-Process -FilePath $ps -ArgumentList $arguments -Wait -PassThru -RedirectStandardError (Join-Path $evidence 'tampered-package-rejected.txt')
    if($test.ExitCode -eq 0){throw 'Tampered runtime was accepted'}
} finally {Remove-Item -LiteralPath $tampered -Recurse -Force}
@{policyCases=$cases.Count;returnCodeCases=$exits.Count;actualSignedPackage=$true;registryAndDllLoader=$true;repeatPrepare=$true;tamperedPackageRejected=$true;limits='CI runtime may already exist. Missing/corrupted/newer-version decisions are injected policy tests, not clean-machine or customer-plugin validation.'} | ConvertTo-Json | Set-Content (Join-Path $evidence 'summary.json') -Encoding UTF8
Write-Host 'VC++ prerequisite policy, actual signed package, loader and repeat checks passed.'
