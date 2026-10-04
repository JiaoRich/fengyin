$ErrorActionPreference = "Stop"
$KitRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$Setup = Join-Path $KitRoot "FengYinDriverSetup.exe"
$Watchdog = Join-Path $KitRoot "FengYinAudioWatchdog.exe"
$CertificatePath = Join-Path $KitRoot "test-driver\TabletAudioSample.cer"

function Assert-Administrator {
    $Identity = [Security.Principal.WindowsIdentity]::GetCurrent()
    $Principal = [Security.Principal.WindowsPrincipal]::new($Identity)
    if (-not $Principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
        Start-Process powershell.exe -Verb RunAs -ArgumentList @(
            "-NoProfile", "-ExecutionPolicy", "Bypass", "-File", "`"$PSCommandPath`""
        )
        exit
    }
}

Assert-Administrator
Get-Process FengYin -ErrorAction SilentlyContinue | ForEach-Object {
    $_.CloseMainWindow() | Out-Null
    if (-not $_.WaitForExit(5000)) { Stop-Process -Id $_.Id -Force }
}
if (Test-Path $Watchdog) {
    & $Watchdog --recover-only
    if ($LASTEXITCODE -ne 0) { throw "无法先恢复 Windows 默认声音设备，已停止卸载以保护系统声音。" }
}

& $Setup --uninstall
if ($LASTEXITCODE -ne 0 -and $LASTEXITCODE -ne 3010) {
    throw "风吟测试音频驱动卸载失败（错误码 $LASTEXITCODE）。"
}
& $Setup --check | Out-Null
if ($LASTEXITCODE -eq 0) { throw "虚拟音频设备仍然存在，测试证书暂不删除。请重启后重试。" }

if (Test-Path $CertificatePath) {
    $Certificate = [System.Security.Cryptography.X509Certificates.X509Certificate2]::new($CertificatePath)
    $Thumbprint = $Certificate.Thumbprint
    Remove-Item "Cert:\LocalMachine\TrustedPublisher\$Thumbprint" -Force -ErrorAction SilentlyContinue
    Remove-Item "Cert:\LocalMachine\Root\$Thumbprint" -Force -ErrorAction SilentlyContinue
}
Remove-Item (Join-Path $env:ProgramData "FengYin\test-driver-cert-thumbprint.txt") -Force -ErrorAction SilentlyContinue

Write-Host "测试驱动和随包测试证书已删除，Windows 默认声音已恢复。" -ForegroundColor Green
Write-Host "如不再测试，请继续运行‘关闭Windows测试模式.ps1’并重启。" -ForegroundColor Yellow
Read-Host "按回车退出"
