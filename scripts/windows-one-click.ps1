$ErrorActionPreference = "Stop"
$ProjectRoot = Split-Path -Parent $PSScriptRoot
$ResultDir = Join-Path $ProjectRoot "构建结果"
$LogFile = Join-Path $ResultDir "构建日志.txt"

New-Item -ItemType Directory -Force -Path $ResultDir | Out-Null
Start-Transcript -Path $LogFile -Force | Out-Null

function Stop-WithHelp([string]$Message) {
    Write-Host "" 
    Write-Host "无法继续：$Message" -ForegroundColor Red
    Write-Host "完整信息已保存到：$LogFile" -ForegroundColor Yellow
    throw $Message
}

try {
    Write-Host "正在检查 Windows 构建环境..." -ForegroundColor Cyan
    if ($env:OS -ne "Windows_NT") { Stop-WithHelp "此文件只能在 Windows 10/11 64 位电脑运行。" }

    $Cmake = Get-Command cmake.exe -ErrorAction SilentlyContinue
    if (-not $Cmake) {
        $BundledCmake = Get-ChildItem "${env:ProgramFiles}\Microsoft Visual Studio\2022" -Filter cmake.exe -Recurse -ErrorAction SilentlyContinue |
            Select-Object -First 1
        if ($BundledCmake) {
            $env:Path = "$($BundledCmake.DirectoryName);$env:Path"
        } else {
            Stop-WithHelp "没有找到 CMake。请安装 Visual Studio 2022 Community，并勾选‘使用 C++ 的桌面开发’。"
        }
    }

    $VsWhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (-not (Test-Path $VsWhere)) {
        Stop-WithHelp "没有找到 Visual Studio 2022。请先安装 Community 免费版。"
    }
    $VsPath = & $VsWhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if (-not $VsPath) {
        Stop-WithHelp "Visual Studio 缺少 C++ 编译组件。请在安装器中勾选‘使用 C++ 的桌面开发’。"
    }

    $InnoCandidates = @(
        "${env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe",
        "$env:ProgramFiles\Inno Setup 6\ISCC.exe"
    )
    if (-not ($InnoCandidates | Where-Object { Test-Path $_ } | Select-Object -First 1)) {
        Stop-WithHelp "没有找到 Inno Setup 6。请安装免费版 Inno Setup 6。"
    }

    Write-Host "环境检查通过，开始编译。请勿关闭窗口..." -ForegroundColor Green
    & (Join-Path $PSScriptRoot "build-windows.ps1")
    if ($LASTEXITCODE -ne 0) { throw "构建脚本返回错误代码 $LASTEXITCODE" }

    $Installer = Get-ChildItem (Join-Path $ProjectRoot "dist\installer") -Filter "*.exe" |
        Sort-Object LastWriteTime -Descending | Select-Object -First 1
    if (-not $Installer) { throw "构建结束，但没有找到安装包。" }

    Copy-Item $Installer.FullName (Join-Path $ResultDir $Installer.Name) -Force
    "构建时间：$(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')`r`n自动测试：全部通过`r`n安装包：$($Installer.Name)" |
        Set-Content (Join-Path $ResultDir "构建成功.txt") -Encoding UTF8
    Write-Host "" 
    Write-Host "全部完成，安装包：$($Installer.Name)" -ForegroundColor Green
    Stop-Transcript | Out-Null
    exit 0
} catch {
    Write-Host "" 
    Write-Host "构建没有完成：$($_.Exception.Message)" -ForegroundColor Red
    Write-Host "请把下面这个文件发回给开发人员：" -ForegroundColor Yellow
    Write-Host $LogFile -ForegroundColor Yellow
    try { Stop-Transcript | Out-Null } catch {}
    exit 1
}
