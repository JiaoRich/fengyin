param([Parameter(Mandatory=$true)][string]$Destination)
$ErrorActionPreference='Stop'
$Expected='D7FC0FB605CBA0B5D359E25CC497C99F292395285F7797964017C28CCC4A83A0'
New-Item -ItemType Directory -Force -Path $Destination | Out-Null
$Package=Join-Path $Destination 'ffpror206x64.exe'
if (-not (Test-Path -LiteralPath $Package)) {
    $TempPackage="$Package.partial"
    try {
        Invoke-WebRequest -UseBasicParsing -TimeoutSec 180 -Uri 'https://cdn-b.fabfilter.com/downloads/ffpror206x64.exe' -OutFile $TempPackage
        if ((Get-FileHash -LiteralPath $TempPackage -Algorithm SHA256).Hash -ne $Expected) { throw 'Official FabFilter installer hash mismatch' }
        $Signature=Get-AuthenticodeSignature -LiteralPath $TempPackage
        if ($Signature.Status -ne 'Valid' -or $Signature.SignerCertificate.Subject -notmatch 'FabFilter') { throw 'Official FabFilter installer signature not verified' }
        Move-Item -LiteralPath $TempPackage -Destination $Package -Force
    } finally { Remove-Item -LiteralPath $TempPackage -Force -ErrorAction SilentlyContinue }
}
if ((Get-FileHash -LiteralPath $Package -Algorithm SHA256).Hash -ne $Expected) { throw 'FabFilter installer hash mismatch' }
$Signature=Get-AuthenticodeSignature -LiteralPath $Package
if ($Signature.Status -ne 'Valid' -or $Signature.SignerCertificate.Subject -notmatch 'FabFilter') { throw 'FabFilter installer signature not verified' }
Copy-Item -LiteralPath (Join-Path $PSScriptRoot '../installer/Install-FabFilter.ps1') -Destination $Destination -Force
@{ source='https://cdn-b.fabfilter.com/downloads/ffpror206x64.exe'; sha256=$Expected; signer=$Signature.SignerCertificate.Subject; productVersion=(Get-Item $Package).VersionInfo.ProductVersion } | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $Destination 'verified-source.json') -Encoding UTF8
'Pro-R 2 is a commercial FabFilter product. This official installer does not include a license. An existing valid license or the official trial is required. https://www.fabfilter.com/download' | Set-Content -LiteralPath (Join-Path $Destination 'License-note.txt') -Encoding UTF8
