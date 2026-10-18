@echo off
setlocal
tasklist /FI "IMAGENAME eq FengYin.exe" /NH | find /I "FengYin.exe" >nul
if not errorlevel 1 (
  echo Close FengYin before changing the recording backend.
  pause
  exit /b 1
)
set "FENGYIN_RECORDING_BACKEND=legacy"
start "" "%~dp0FengYin.exe"
