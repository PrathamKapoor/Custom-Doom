@echo off
set "PROJECT_ROOT=%~dp0.."
if not exist "%PROJECT_ROOT%\.venv\Scripts\pythonw.exe" (
  echo Run desktop\setup.ps1 first to install the control panel.
  pause
  exit /b 1
)
start "" "%PROJECT_ROOT%\.venv\Scripts\pythonw.exe" "%~dp0control_panel.py"
