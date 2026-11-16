param([string]$Installer = '')
$ErrorActionPreference = 'Stop'
# Never install/update a developer's or customer's runtime from this test helper.
if ($env:GITHUB_ACTIONS -ne 'true') { throw 'This helper is restricted to the disposable GitHub runner.' }
$cache = Join-Path (Split-Path -Parent $PSScriptRoot) 'third_party\ci-webview\WebView2RuntimeInstallerX64.exe'
# Audited official Microsoft object, 2026-11-16. Do not use the mutable latest link.
$expectedSha = 'd3cbe8fedb104282d745fff106993efead51ae94f5595a4e75804446a974f9c3'
$installerUrl = 'https://msedge.sf.dl.delivery.mp.microsoft.com/filestreamingservice/files/52cfd0a8-f256-48d4-9b18-467f2b3cda78/MicrosoftEdgeWebView2RuntimeInstallerX64.exe'
try {
    if (-not $Installer) {
        $Installer = $cache
        if (-not (Test-Path -LiteralPath $Installer)) {
            New-Item -ItemType Directory -Force -Path (Split-Path -Parent $cache) | Out-Null
            Invoke-WebRequest -UseBasicParsing -TimeoutSec 300 -Uri $installerUrl -OutFile $Installer
        }
    }
    if (-not (Test-Path -LiteralPath $Installer) -or (Get-Item -LiteralPath $Installer).Length -lt 100MB) { throw 'Incomplete WebView2 offline installer' }
    $signature = Get-AuthenticodeSignature -FilePath $Installer
    if ($signature.Status -ne 'Valid' -or $signature.SignerCertificate.Subject -notmatch 'Microsoft Corporation') { throw 'WebView2 Microsoft signature check failed' }
    $installerSha = (Get-FileHash -LiteralPath $Installer -Algorithm SHA256).Hash.ToLowerInvariant()
    if ($installerSha -ne $expectedSha) { throw 'Pinned WebView2 installer SHA mismatch; re-audit the upstream package, never silently mix runtime versions.' }
    if ($Installer -ne $cache) {
        New-Item -ItemType Directory -Force -Path (Split-Path -Parent $cache) | Out-Null
        Copy-Item -LiteralPath $Installer -Destination $cache -Force
    }
    $install = Start-Process -FilePath $Installer -ArgumentList @('/silent', '/install') -PassThru
    if (-not $install.WaitForExit(300000)) { $install.Kill(); throw 'CI WebView2 installation timed out' }
    $install.Refresh()
    if ($install.ExitCode -notin @(0,3010)) { throw ('CI WebView2 install exit code: ' + $install.ExitCode) }
    $candidates = @()
    foreach ($base in @(${env:ProgramFiles(x86)}, $env:ProgramFiles, $env:LOCALAPPDATA)) {
        if (-not $base) { continue }
        $root = Join-Path $base 'Microsoft\EdgeWebView\Application'
        foreach ($folder in @(Get-ChildItem -LiteralPath $root -Directory -ErrorAction SilentlyContinue)) {
            if ($folder.Name -match '^\d+\.\d+\.\d+\.\d+$' -and (Test-Path -LiteralPath (Join-Path $folder.FullName 'msedgewebview2.exe'))) {
                $candidates += [pscustomobject]@{ Version=[version]$folder.Name; Path=$folder.FullName }
            }
        }
    }
    $runtime = $candidates | Sort-Object Version -Descending | Select-Object -First 1
    if (-not $runtime -or $runtime.Version.Major -lt 155) { throw 'CI must test a current runtime comparable to the captured target (155+), not the runner image 131.' }
    # This is an installed Evergreen runtime, not a Fixed Version deployment.
    # Use the same registered-runtime discovery as the shipping application.
    # Pinning a versioned installation directory becomes stale if Edge Update
    # replaces it during the long native build. Never change machine settings.
    Remove-Item Env:WEBVIEW2_BROWSER_EXECUTABLE_FOLDER -ErrorAction SilentlyContinue
    [pscustomobject]@{version=$runtime.Version.ToString();path=$runtime.Path;selection='registered-evergreen';installerSha256=$installerSha;signature='Microsoft Corporation';exitCode=$install.ExitCode} |
        ConvertTo-Json | Set-Content -Encoding UTF8 'ci-webview-runtime.json'
    Write-Host ('Verified CI WebView2 runtime: ' + $runtime.Version)
} catch { throw }
