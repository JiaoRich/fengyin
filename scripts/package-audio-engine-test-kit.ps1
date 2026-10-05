param(
    [Parameter(Mandatory = $true)]
    [string]$AppPackageDir,
    [Parameter(Mandatory = $true)]
    [string]$DriverDir,
    [Parameter(Mandatory = $true)]
    [string]$OutputDir
)

$ErrorActionPreference = "Stop"
$ProjectRoot = Split-Path -Parent $PSScriptRoot
$AppPackageDir = [System.IO.Path]::GetFullPath($AppPackageDir)
$DriverDir = [System.IO.Path]::GetFullPath($DriverDir)
$OutputDir = [System.IO.Path]::GetFullPath($OutputDir)

function Copy-PowerShellScriptAsUtf8Bom {
    param(
        [Parameter(Mandatory = $true)][string]$Source,
        [Parameter(Mandatory = $true)][string]$Destination
    )

    $Utf8WithoutBom = [System.Text.UTF8Encoding]::new($false)
    $Utf8WithBom = [System.Text.UTF8Encoding]::new($true)
    $Contents = [System.IO.File]::ReadAllText($Source, $Utf8WithoutBom)
    [System.IO.File]::WriteAllText($Destination, $Contents, $Utf8WithBom)
}

& (Join-Path $PSScriptRoot "test-fengyin-test-driver.ps1") -DriverDir $DriverDir
if (-not $?) { throw "测试驱动验证失败" }

if (Test-Path $OutputDir) { Remove-Item $OutputDir -Recurse -Force }
New-Item -ItemType Directory -Force -Path $OutputDir | Out-Null
Copy-Item (Join-Path $AppPackageDir "*") $OutputDir -Recurse -Force

$PackagedDriver = Join-Path $OutputDir "test-driver"
New-Item -ItemType Directory -Force -Path $PackagedDriver | Out-Null
foreach ($Name in @("FengYinAudio.inf", "FengYinAudio.cat", "TabletAudioSample.sys", "TabletAudioSample.cer")) {
    Copy-Item (Join-Path $DriverDir $Name) (Join-Path $PackagedDriver $Name) -Force
}

foreach ($ScriptName in @(
    "Enable-TestMode.ps1",
    "Install-TestAudioEngine.ps1",
    "Uninstall-TestAudioEngine.ps1",
    "Disable-TestMode.ps1"
)) {
    Copy-PowerShellScriptAsUtf8Bom `
        -Source (Join-Path $ProjectRoot "scripts\audio-test-kit\$ScriptName") `
        -Destination (Join-Path $OutputDir $ScriptName)
}
Copy-Item (Join-Path $ProjectRoot "scripts\audio-test-kit\*.cmd") $OutputDir -Force
Copy-Item (Join-Path $ProjectRoot "docs\风吟音频引擎免费实机测试说明.md") (Join-Path $OutputDir "测试说明-请先阅读.md") -Force

foreach ($ScriptName in @(
    "Enable-TestMode.ps1",
    "Install-TestAudioEngine.ps1",
    "Uninstall-TestAudioEngine.ps1",
    "Disable-TestMode.ps1"
)) {
    $ScriptPath = Join-Path $OutputDir $ScriptName
    & powershell.exe -NoProfile -NonInteractive -Command `
        "[void][scriptblock]::Create((Get-Content -Raw -LiteralPath '$ScriptPath'))"
    if ($LASTEXITCODE -ne 0) {
        throw "Windows PowerShell 5.1 语法检查失败：$ScriptName"
    }
}

Write-Host "免费实机测试包已生成：$OutputDir" -ForegroundColor Green
