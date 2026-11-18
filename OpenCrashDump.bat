@echo off
setlocal DisableDelayedExpansion
chcp 65001 >nul
rem 용도: Dump를 드래그하면 서버 선택·인증·debugger 설정 후 별도 Visual Studio로 엽니다.
rem Windows 사용자 환경 변수는 저장하지 않으며, 이 batch와 새 VS process에만 적용합니다.

if "%~1"=="" goto usage
if not "%~2"=="" goto usage
if /I not "%~x1"==".dmp" goto usage
if not exist "%~f1" (
    echo Dump file not found: "%~f1"
    goto failed
)

rem 작업 파일 대신 Git commit의 내용을 읽으므로 저장소 루트와 helper 위치를 전달합니다.
for %%I in ("%~dp0.") do set "HITORI_SOURCE_REPO=%%~fI"
set "HITORI_SOURCE_TOOLS=%HITORI_SOURCE_REPO%\Setup"
git -C "%HITORI_SOURCE_REPO%" rev-parse --git-dir >nul 2>&1
if errorlevel 1 (
    echo Git is required. Run this batch from a Git clone of the engine repository.
    goto failed
)
if not exist "%HITORI_SOURCE_TOOLS%\GetIndexedSource.ps1" goto missingHelpers
if not exist "%HITORI_SOURCE_TOOLS%\GitSource.ps1" goto missingHelpers
if not exist "%HITORI_SOURCE_TOOLS%\OpenCrashDump.ps1" goto missingHelpers

rem Build Tools가 아닌 C++ 도구가 설치된 VS IDE를 찾습니다. Insiders 설치도 포함합니다.
set "VsWhere=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VsWhere%" (
    echo Visual Studio Installer was not found. Install Visual Studio with C++ tools.
    goto failed
)
set "VisualStudioExe="
for /f "usebackq delims=" %%I in (`"%VsWhere%" -latest -prerelease -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -find Common7\IDE\devenv.exe`) do set "VisualStudioExe=%%I"
if not defined VisualStudioExe (
    echo Visual Studio IDE with C++ tools was not found.
    goto failed
)

rem 입력·인증·XML 생성은 PowerShell에서 처리합니다. 비밀번호는 batch에 전달하지 않습니다.
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%HITORI_SOURCE_TOOLS%\OpenCrashDump.ps1" -DumpPath "%~f1" -VisualStudioPath "%VisualStudioExe%"
if errorlevel 1 goto failed
exit /b 0

:missingHelpers
echo Source retrieval helpers were not found under "%HITORI_SOURCE_TOOLS%".
goto failed

:usage
echo Drag one .dmp file onto OpenCrashDump.bat.
echo Or run: OpenCrashDump.bat "C:\path\Crash.dmp"

:failed
pause
exit /b 1
