@echo off
setlocal
if /I "%~1"=="--no-pause" goto unattended
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0SetupCrashTools.ps1" %*
set "SetupExitCode=%ERRORLEVEL%"
echo.
pause
exit /b %SetupExitCode%

:unattended
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0SetupCrashTools.ps1" %2 %3 %4 %5 %6 %7 %8 %9
exit /b %ERRORLEVEL%
