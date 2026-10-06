param([Parameter(Mandatory=$true)][string]$Destination)
$ErrorActionPreference = 'Stop'
New-Item -ItemType Directory -Force -Path $Destination | Out-Null
$installer = Join-Path $Destination 'ASIO4ALL_2_22.exe'
Invoke-WebRequest 'https://asio4all.org/downloads/ASIO4ALL_2_22.exe' -OutFile $installer -UseBasicParsing
if ((Get-FileHash $installer -Algorithm SHA256).Hash -ne '0D4F0C63BF5DF077E4C74F18372F72B8E875A9ABEA499FEA78AAA9F56022AC7E') {
    throw 'ASIO4ALL installer hash mismatch; refusing to package changed installer.'
}
# Redistribution authorization is managed by the product owner.
# Keep the original interactive installer; never accept its terms silently.
