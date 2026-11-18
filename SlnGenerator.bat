@echo off
setlocal
rem Crash 도구 설치 실패는 경고하고 기존 solution 생성은 계속합니다.
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0Setup\SetupCrashTools.ps1"
set "CrashToolsExitCode=%ERRORLEVEL%"
if "%CrashToolsExitCode%"=="3010" echo [INFO] Crash tools setup requires a Windows restart.
if not "%CrashToolsExitCode%"=="0" if not "%CrashToolsExitCode%"=="3010" (
    echo [WARNING] Crash tools setup failed. Continuing solution generation.
    echo [WARNING] Symbol publishing needs these tools. Retry Setup\SetupCrashTools.bat later.
    echo [WARNING] If Windows SDK headers or libraries are missing, engine builds also need SDK setup.
)

powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0Setup\SetupFbxSdk.ps1" -GenerateProjects
set "GenerateExitCode=%ERRORLEVEL%"
echo.
if /I not "%~1"=="--no-pause" pause
exit /b %GenerateExitCode%
