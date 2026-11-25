@echo off
setlocal
set "ps=%SystemRoot%\System32\WindowsPowerShell\v1.0\powershell.exe"
if exist "%SystemRoot%\Sysnative\WindowsPowerShell\v1.0\powershell.exe" set "ps=%SystemRoot%\Sysnative\WindowsPowerShell\v1.0\powershell.exe"
"%ps%" -NoLogo -NoProfile -STA -ExecutionPolicy Bypass -File "%~dp0Start.ps1"
if errorlevel 1 pause
