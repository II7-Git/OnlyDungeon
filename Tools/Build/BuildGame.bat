@echo off
rem ============================================================
rem  OnlyDungeon - Build the game target (not the editor target).
rem  Usage: BuildGame.bat [Development|Shipping]   (default: Development)
rem
rem  Works while the editor is open with Live Coding, because only
rem  the editor target is locked. Use it to check Shipping-only code
rem  (UE_BUILD_SHIPPING) or to verify a build without closing the editor.
rem
rem  Output: Saved\ValidateBuild\Game_<Config>.log / .rc (0 = success)
rem  Engine lookup: UE_ROOT env var -> registry -> default install path
rem  (same as ValidateBuild.bat). Messages stay ASCII for cmd.exe.
rem ============================================================
setlocal EnableExtensions

set "CONFIG=%~1"
if "%CONFIG%"=="" set "CONFIG=Development"
set "ENGINE_VERSION=5.9"
for %%I in ("%~dp0..\..") do set "PROJECT_DIR=%%~fI"
set "PROJECT_FILE=%PROJECT_DIR%\OnlyDungeon.uproject"
set "LOG_DIR=%PROJECT_DIR%\Saved\ValidateBuild"
set "ENGINE_DIR="

if not exist "%PROJECT_FILE%" (echo [ENV] Project not found: %PROJECT_FILE% & exit /b 2)
if defined UE_ROOT set "ENGINE_DIR=%UE_ROOT%"
if defined ENGINE_DIR goto :engine_found
for /f "tokens=2,*" %%A in ('reg query "HKLM\SOFTWARE\EpicGames\Unreal Engine\%ENGINE_VERSION%" /v InstalledDirectory 2^>nul ^| find /i "InstalledDirectory"') do set "ENGINE_DIR=%%B"
if defined ENGINE_DIR goto :engine_found
if exist "C:\Program Files\Epic Games\UE_%ENGINE_VERSION%\Engine" set "ENGINE_DIR=C:\Program Files\Epic Games\UE_%ENGINE_VERSION%"
:engine_found
if not defined ENGINE_DIR (echo [ENV] Unreal Engine %ENGINE_VERSION% not found. Set UE_ROOT. & exit /b 2)

if not exist "%LOG_DIR%" mkdir "%LOG_DIR%"
set "LOG_FILE=%LOG_DIR%\Game_%CONFIG%.log"
echo [INFO] Target : OnlyDungeon Win64 %CONFIG%
echo [INFO] Log    : %LOG_FILE%
call "%ENGINE_DIR%\Engine\Build\BatchFiles\Build.bat" OnlyDungeon Win64 %CONFIG% -Project="%PROJECT_FILE%" -WaitMutex > "%LOG_FILE%" 2>&1
set "RC=%ERRORLEVEL%"
echo %RC% > "%LOG_DIR%\Game_%CONFIG%.rc"
if "%RC%"=="0" (echo [SUCCESS] OnlyDungeon Win64 %CONFIG% build succeeded.) else (echo [FAIL] Build failed. See %LOG_FILE%)
exit /b %RC%
