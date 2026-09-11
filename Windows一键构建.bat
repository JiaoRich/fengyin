@echo off
chcp 65001 >nul
setlocal
cd /d "%~dp0"
title 风吟 Windows 一键构建

echo.
echo ============================================================
echo                 风吟 Windows 一键构建
echo ============================================================
echo.
echo 本工具会自动检查环境、编译、测试并生成中文安装包。
echo 第一次构建可能需要 15 至 40 分钟，请保持网络连接。
echo.

powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0scripts\windows-one-click.ps1"
set "RESULT=%ERRORLEVEL%"

echo.
if "%RESULT%"=="0" (
    echo [成功] 安装包已生成，请查看“构建结果”文件夹。
) else (
    echo [未完成] 请把“构建结果\构建日志.txt”复制回来发给我。
)
echo.
pause
exit /b %RESULT%
