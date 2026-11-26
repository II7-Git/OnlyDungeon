@echo off
rem ============================================================
rem  OnlyDungeon - Package the Windows client (build + cook + pak).
rem  Usage: PackageGame.bat [Shipping|Development]   (default: Shipping)
rem
rem  -nocompileeditor: an open editor with Live Coding blocks the
rem  editor target build, so only the game target is compiled here.
rem  -map: cook only the real game map (no MapsToCook in config,
rem  otherwise template and pack demo maps get cooked too).
rem
rem  Output : Saved\Package\<Config>\Windows\OnlyDungeon.exe
rem  Log    : Saved\Package\BuildCookRun_<Config>.log / .rc (0 = success)
rem  Note   : close the packaged game completely before packaging again
rem           (Binaries\Win64\OnlyDungeon-Win64-<Config>.exe can stay alive).
rem ============================================================
setlocal EnableExtensions

set "CONFIG=%~1"
if "%CONFIG%"=="" set "CONFIG=Shipping"
set "ENGINE_VERSION=5.9"
set "GAME_MAP=/Game/OnlyDungeon/Maps/L_OD_Dungeon_01"
for %%I in ("%~dp0..\..") do set "PROJECT_DIR=%%~fI"
set "PROJECT_FILE=%PROJECT_DIR%\OnlyDungeon.uproject"
set "OUT_DIR=%PROJECT_DIR%\Saved\Package"
set "ENGINE_DIR="

if not exist "%PROJECT_FILE%" (echo [ENV] Project not found: %PROJECT_FILE% & exit /b 2)
if defined UE_ROOT set "ENGINE_DIR=%UE_ROOT%"
if defined ENGINE_DIR goto :engine_found
for /f "tokens=2,*" %%A in ('reg query "HKLM\SOFTWARE\EpicGames\Unreal Engine\%ENGINE_VERSION%" /v InstalledDirectory 2^>nul ^| find /i "InstalledDirectory"') do set "ENGINE_DIR=%%B"
if defined ENGINE_DIR goto :engine_found
if exist "C:\Program Files\Epic Games\UE_%ENGINE_VERSION%\Engine" set "ENGINE_DIR=C:\Program Files\Epic Games\UE_%ENGINE_VERSION%"
:engine_found
if not defined ENGINE_DIR (echo [ENV] Unreal Engine %ENGINE_VERSION% not found. Set UE_ROOT. & exit /b 2)

if not exist "%OUT_DIR%" mkdir "%OUT_DIR%"
set "LOG_FILE=%OUT_DIR%\BuildCookRun_%CONFIG%.log"
echo [INFO] Config : %CONFIG%
echo [INFO] Output : %OUT_DIR%\%CONFIG%
echo [INFO] Log    : %LOG_FILE%
call "%ENGINE_DIR%\Engine\Build\BatchFiles\RunUAT.bat" BuildCookRun -project="%PROJECT_FILE%" -noP4 -platform=Win64 -clientconfig=%CONFIG% -build -nocompileeditor -cook -stage -pak -archive -archivedirectory="%OUT_DIR%\%CONFIG%" -map=%GAME_MAP% -utf8output -unattended > "%LOG_FILE%" 2>&1
set "RC=%ERRORLEVEL%"
echo %RC% > "%OUT_DIR%\BuildCookRun_%CONFIG%.rc"
if "%RC%"=="0" (echo [SUCCESS] Package created: %OUT_DIR%\%CONFIG%\Windows) else (echo [FAIL] Packaging failed. See %LOG_FILE%)
exit /b %RC%
