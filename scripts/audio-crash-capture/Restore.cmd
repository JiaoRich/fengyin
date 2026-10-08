@echo off
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0Capture.ps1" -RestoreOnly
pause
