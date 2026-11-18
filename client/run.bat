@echo off
cd /d "%~dp0"
if "%~1"=="" (
  set /p PORT=Enter COM port, for example COM5:
) else (
  set PORT=%~1
)
python compass_client.py %PORT%
if errorlevel 1 pause
