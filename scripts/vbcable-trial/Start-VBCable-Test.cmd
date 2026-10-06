@echo off
setlocal
tasklist /FI "IMAGENAME eq FengYin.exe" /NH | find /I "FengYin.exe" >nul
if not errorlevel 1 (
  echo Close FengYin first, then run this launcher again.
  pause
  exit /b 1
)
if not exist "%~dp0FengYin.exe" (
  echo Extract the complete package before running this launcher.
  pause
  exit /b 1
)
set "FENGYIN_VBCABLE_TEST=1"
start "" "%~dp0FengYin.exe"
