param([Parameter(Mandatory=$true)][string]$Destination)

$ErrorActionPreference = 'Stop'
$ProjectRoot = Split-Path -Parent $PSScriptRoot
$Source = Join-Path $ProjectRoot 'third_party\fresh-air\Setup Fresh Air v1.0.8.exe'
$ExpectedHash = 'C50D3CE92ACB7524B1A4C962F9ADFCCE100FBEB957715B8788051D73F0D02A73'

if (-not (Test-Path $Source)) {
    throw "Fresh Air 1.0.8 installer is missing: $Source"
}
if ((Get-FileHash $Source -Algorithm SHA256).Hash -ne $ExpectedHash) {
    throw 'Fresh Air 1.0.8 installer SHA-256 mismatch; packaging refused.'
}

New-Item -ItemType Directory -Force -Path $Destination | Out-Null
Copy-Item $Source (Join-Path $Destination 'Setup Fresh Air v1.0.8.exe') -Force
