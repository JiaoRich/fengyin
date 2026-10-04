$ErrorActionPreference = "Stop"

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

$SecureBootEnabled = $false
try { $SecureBootEnabled = Confirm-SecureBootUEFI } catch { }
if ($SecureBootEnabled) {
    Write-Host "检测到安全启动（Secure Boot）仍在启用。" -ForegroundColor Yellow
    Write-Host "Windows 不允许在安全启动开启时启用测试签名。请先保存 BitLocker 恢复密钥，暂停 BitLocker，并在 BIOS/UEFI 中暂时关闭安全启动。" -ForegroundColor Yellow
    Write-Host "风吟不会自动修改 BIOS、BitLocker 或安全启动设置。" -ForegroundColor Yellow
    exit 2
}

Write-Host "即将为这台测试电脑开启 Windows 测试签名模式。" -ForegroundColor Yellow
Write-Host "影响：桌面会出现‘测试模式’水印；仅建议用于备用实体测试电脑；完成后可以完整关闭。" -ForegroundColor Yellow
$Answer = Read-Host "输入 TEST 后继续"
if ($Answer -cne "TEST") { Write-Host "已取消。"; exit 1 }

& bcdedit.exe /set testsigning on
if ($LASTEXITCODE -ne 0) {
    throw "Windows 未能开启测试签名模式（错误码 $LASTEXITCODE）。请确认安全启动已关闭。"
}

Write-Host "测试签名模式已设置。请重启电脑，然后运行‘安装测试音频引擎.ps1’。" -ForegroundColor Green
Read-Host "按回车退出"
