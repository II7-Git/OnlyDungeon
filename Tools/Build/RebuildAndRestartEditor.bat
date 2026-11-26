@echo off
rem ============================================================
rem  OnlyDungeon - Wait for the editor to exit, run ValidateBuild.bat
rem  (editor target), then start the editor again.
rem  Usage: RebuildAndRestartEditor.bat <EditorPID>
rem
rem  Use it after header / UCLASS changes that Live Coding cannot apply.
rem  Launch it detached (e.g. WMI Win32_Process Create) so it survives
rem  the editor exit, then quit the editor.
rem  Status: Saved\ValidateBuild\Restart.status (waiting / building /
rem          failed N / launching / done), Restart.rc, Restart.log
rem ============================================================
setlocal EnableExtensions

set "PID=%~1"
set "ENGINE_VERSION=5.9"
for %%I in ("%~dp0..\..") do set "D=%%~fI"
set "S=%D%\Saved\ValidateBuild"
if "%PID%"=="" (echo Usage: RebuildAndRestartEditor.bat EditorPID & exit /b 2)
if not exist "%S%" mkdir "%S%"

set "ENGINE_DIR="
if defined UE_ROOT set "ENGINE_DIR=%UE_ROOT%"
if defined ENGINE_DIR goto :engine_found
for /f "tokens=2,*" %%A in ('reg query "HKLM\SOFTWARE\EpicGames\Unreal Engine\%ENGINE_VERSION%" /v InstalledDirectory 2^>nul ^| find /i "InstalledDirectory"') do set "ENGINE_DIR=%%B"
if defined ENGINE_DIR goto :engine_found
if exist "C:\Program Files\Epic Games\UE_%ENGINE_VERSION%\Engine" set "ENGINE_DIR=C:\Program Files\Epic Games\UE_%ENGINE_VERSION%"
:engine_found

del "%S%\Restart.rc" 2>nul
echo waiting %PID% > "%S%\Restart.status"
:wait
ping -n 3 127.0.0.1 >nul
tasklist /FI "PID eq %PID%" /NH 2>nul | find " %PID% " >nul
if not errorlevel 1 goto wait
rem Live Coding console can outlive the editor for a moment.
rem (ping is used for delays: timeout.exe fails when stdin is redirected)
:waitlc
tasklist /FI "IMAGENAME eq LiveCodingConsole.exe" 2>nul | find /I "LiveCodingConsole.exe" >nul
if errorlevel 1 goto build
ping -n 3 127.0.0.1 >nul
goto waitlc
:build
echo building > "%S%\Restart.status"
call "%D%\ValidateBuild.bat" > "%S%\Restart.log" 2>&1
set RC=%ERRORLEVEL%
echo %RC% > "%S%\Restart.rc"
if not "%RC%"=="0" (
  echo failed %RC% > "%S%\Restart.status"
  exit /b %RC%
)
echo launching > "%S%\Restart.status"
if not defined ENGINE_DIR (
  echo failed noengine > "%S%\Restart.status"
  exit /b 2
)
start "" "%ENGINE_DIR%\Engine\Binaries\Win64\UnrealEditor.exe" "%D%\OnlyDungeon.uproject"
echo done > "%S%\Restart.status"
