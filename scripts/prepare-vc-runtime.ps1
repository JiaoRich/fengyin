param([Parameter(Mandatory=$true)][string]$DestinationDirectory)
$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
$source=Join-Path $root 'installer'
$manifest=Get-Content -LiteralPath (Join-Path $source 'vc-runtime.json') -Raw | ConvertFrom-Json
New-Item -ItemType Directory -Force -Path $DestinationDirectory | Out-Null
$target=Join-Path $DestinationDirectory $manifest.file
if (-not (Test-Path -LiteralPath $target) -or (Get-FileHash -LiteralPath $target -Algorithm SHA256).Hash -ne $manifest.sha256) {
    $partial=$target+'.part'
    try {
        Invoke-WebRequest -UseBasicParsing -TimeoutSec 300 -Uri $manifest.url -OutFile $partial
        if ((Get-FileHash -LiteralPath $partial -Algorithm SHA256).Hash -ne $manifest.sha256) { throw 'Official VC++ download hash mismatch' }
        Move-Item -LiteralPath $partial -Destination $target -Force
    } finally { if(Test-Path -LiteralPath $partial){Remove-Item -LiteralPath $partial -Force} }
}
foreach($name in @('vc-runtime.json','VCRuntimePolicy.ps1','Install-VCRuntime.ps1')){Copy-Item -LiteralPath (Join-Path $source $name) -Destination $DestinationDirectory -Force}
& "$env:SystemRoot\System32\WindowsPowerShell\v1.0\powershell.exe" -NoProfile -NonInteractive -ExecutionPolicy Bypass -File (Join-Path $DestinationDirectory 'Install-VCRuntime.ps1') -PackageDirectory $DestinationDirectory -VerifyOnly
if($LASTEXITCODE -ne 0){throw 'Official VC++ package signature/version verification failed'}
