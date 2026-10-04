$ErrorActionPreference = "Stop"
$KitRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$Setup = Join-Path $KitRoot "FengYinDriverSetup.exe"

$Identity = [Security.Principal.WindowsIdentity]::GetCurrent()
$Principal = [Security.Principal.WindowsPrincipal]::new($Identity)
if (-not $Principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
    Start-Process powershell.exe -Verb RunAs -ArgumentList @(
        "-NoProfile", "-ExecutionPolicy", "Bypass", "-File", "`"$PSCommandPath`""
    )
    exit
}
if (Test-Path $Setup) {
    & $Setup --check | Out-Null
    if ($LASTEXITCODE -eq 0) {
        throw "请先双击‘3-卸载并恢复系统声音.cmd’，确认测试驱动已删除后再关闭测试模式。"
    }
}

& bcdedit.exe /set testsigning off
if ($LASTEXITCODE -ne 0) { throw "Windows 未能关闭测试签名模式（错误码 $LASTEXITCODE）。" }
Write-Host "测试签名模式已关闭。请重启电脑使设置生效。" -ForegroundColor Green
Write-Host "如果测试前手动关闭过安全启动，请在确认系统正常后自行重新开启。" -ForegroundColor Yellow
Read-Host "按回车退出"
