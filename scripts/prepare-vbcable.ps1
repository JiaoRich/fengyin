param([Parameter(Mandatory=$true)][string]$Destination)
$ErrorActionPreference = 'Stop'
$archive = Join-Path $env:TEMP ('fengyin-vbcable-' + [guid]::NewGuid().ToString('N') + '.zip')
try {
    Invoke-WebRequest 'https://download.vb-audio.com/Download_CABLE/VBCABLE_Driver_Pack45.zip' -OutFile $archive -UseBasicParsing
    if ((Get-FileHash $archive -Algorithm SHA256).Hash -ne 'B950E39F01AF1D04EA623C8F6D8EB9B6EA5C477C637295FABF20631C85116BFB') {
        throw 'VB-CABLE archive hash mismatch; refusing to package changed driver.'
    }
    Expand-Archive $archive -DestinationPath $Destination -Force
    if (-not (Test-Path (Join-Path $Destination 'VBCABLE_Setup_x64.exe'))) {
        throw 'VB-CABLE x64 installer missing.'
    }
} finally {
    if (Test-Path $archive) { Remove-Item -LiteralPath $archive }
}
