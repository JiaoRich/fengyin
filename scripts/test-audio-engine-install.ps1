param(
    [Parameter(Mandatory = $true)]
    [string]$PackageDir,
    [switch]$KeepInstalled
)

$ErrorActionPreference = "Stop"
$Identity = [Security.Principal.WindowsIdentity]::GetCurrent()
$Principal = New-Object Security.Principal.WindowsPrincipal($Identity)
if (-not $Principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
    throw "请以管理员身份运行此验收脚本。"
}

$PackageDir = [System.IO.Path]::GetFullPath($PackageDir)
$Setup = Join-Path $PackageDir "FengYinDriverSetup.exe"
$Engine = Join-Path $PackageDir "FengYinAudioEngine.exe"
$Watchdog = Join-Path $PackageDir "FengYinAudioWatchdog.exe"
$Inf = Join-Path $PackageDir "driver\FengYinAudio.inf"
foreach ($Path in @($Setup, $Engine, $Watchdog, $Inf)) {
    if (-not (Test-Path $Path -PathType Leaf)) { throw "验收包缺少：$Path" }
}

$InstalledByTest = $false
try {
    & $Setup --check
    if ($LASTEXITCODE -ne 0) {
        & $Setup --install $Inf
        if ($LASTEXITCODE -ne 0 -and $LASTEXITCODE -ne 3010) {
            throw "虚拟音频驱动安装失败：$LASTEXITCODE"
        }
        $InstalledByTest = $true
    }
    & $Setup --check
    if ($LASTEXITCODE -ne 0) { throw "驱动安装后未发现 Root\FengYinAudioEngine。" }

    $Process = Start-Process $Engine -ArgumentList "--buffer 256 --route-system-audio" -PassThru
    Start-Sleep -Seconds 4
    if ($Process.HasExited) { throw "独立音频引擎启动后异常退出：$($Process.ExitCode)" }
    Stop-Process -Id $Process.Id -Force
    Wait-Process -Id $Process.Id -ErrorAction SilentlyContinue
    & $Watchdog --recover-only
    if ($LASTEXITCODE -ne 0) { throw "强制结束引擎后，默认输出恢复失败。" }

    Write-Host "安装/启动/异常恢复冒烟验收通过。仍需按实机清单验证浏览器混音、延迟、爆音和耳机插拔。" -ForegroundColor Green
} finally {
    & $Watchdog --recover-only | Out-Null
    if ($InstalledByTest -and -not $KeepInstalled) {
        & $Setup --uninstall | Out-Null
        & $Setup --check | Out-Null
        if ($LASTEXITCODE -eq 0) { throw "验收清理失败：虚拟音频设备仍然存在。" }
    }
}

