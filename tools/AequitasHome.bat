@echo off
setlocal
cd /d "%~dp0\.."

where py >nul 2>nul
if %ERRORLEVEL%==0 (
  py -3 "%~dp0homebase.py"
  exit /b %ERRORLEVEL%
)

where python >nul 2>nul
if %ERRORLEVEL%==0 (
  python "%~dp0homebase.py"
  exit /b %ERRORLEVEL%
)

echo Python 3 with tkinter is required to open Aequitas Homebase.
echo Install from https://www.python.org/downloads/ ^(check "tcl/tk"^) or the Microsoft Store.
pause
exit /b 1
