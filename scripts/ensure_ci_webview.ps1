param([string]$Installer = '')
$ErrorActionPreference = 'Stop'
# Never install/update a developer's or customer's runtime from this test helper.
if ($env:GITHUB_ACTIONS -ne 'true') { throw 'This helper is restricted to the disposable GitHub runner.' }
$ownedDownload = $false
try {
    if (-not $Installer) {
        $Installer = Join-Path $env:TEMP ('fy-ci-webview-' + [guid]::NewGuid().ToString('N') + '.exe')
        $ownedDownload = $true
        Invoke-WebRequest -UseBasicParsing -TimeoutSec 300 -Uri 'https://go.microsoft.com/fwlink/?linkid=2124701' -OutFile $Installer
    }
    if (-not (Test-Path -LiteralPath $Installer) -or (Get-Item -LiteralPath $Installer).Length -lt 100MB) { throw 'Incomplete WebView2 offline installer' }
    $signature = Get-AuthenticodeSignature -FilePath $Installer
    if ($signature.Status -ne 'Valid' -or $signature.SignerCertificate.Subject -notmatch 'Microsoft Corporation') { throw 'WebView2 Microsoft signature check failed' }
    $installerSha = (Get-FileHash -LiteralPath $Installer -Algorithm SHA256).Hash.ToLowerInvariant()
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
    # Make the exact selected runtime explicit for child diagnostic processes.
    $env:WEBVIEW2_BROWSER_EXECUTABLE_FOLDER = $runtime.Path
    [pscustomobject]@{version=$runtime.Version.ToString();path=$runtime.Path;installerSha256=$installerSha;signature='Microsoft Corporation';exitCode=$install.ExitCode} |
        ConvertTo-Json | Set-Content -Encoding UTF8 'ci-webview-runtime.json'
    Write-Host ('Verified CI WebView2 runtime: ' + $runtime.Version)
} finally {
    if ($ownedDownload -and (Test-Path -LiteralPath $Installer)) { Remove-Item -LiteralPath $Installer -Force }
}
