@echo off
cd /d "%~dp0"
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0Collect-Audio.ps1"
if errorlevel 1 echo Collection failed. Please send a screenshot of this window.
pause
