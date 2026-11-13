@echo off
rem ============================================================
rem  OnlyDungeon - Build validation script (UE 5.9)
rem  Builds OnlyDungeonEditor Win64 Development through UBT.
rem
rem  Exit codes:
rem    0 = [SUCCESS]    build succeeded
rem    1 = [FAIL]       compile / link error
rem    2 = [ENV]        engine or project not found
rem    3 = [LIVECODING] Live Coding is active in a running editor
rem
rem  Engine lookup order:
rem    1) UE_ROOT environment variable (engine root folder)
rem    2) Registry: HKLM\SOFTWARE\EpicGames\Unreal Engine\5.9
rem    3) C:\Program Files\Epic Games\UE_5.9
rem
rem  Messages are kept in English on purpose: cmd.exe reads .bat
rem  files in the console code page, so non-ASCII text can break.
rem ============================================================
setlocal EnableExtensions

set "ENGINE_VERSION=5.9"
set "TARGET=OnlyDungeonEditor"
set "PLATFORM=Win64"
set "CONFIG=Development"
set "PROJECT_DIR=%~dp0"
set "PROJECT_FILE=%PROJECT_DIR%OnlyDungeon.uproject"
set "LOG_DIR=%PROJECT_DIR%Saved\ValidateBuild"
set "LOG_FILE=%LOG_DIR%\LastBuild.log"
set "ENGINE_DIR="

if not exist "%PROJECT_FILE%" goto :err_project

rem --- 1) UE_ROOT environment variable ---
if defined UE_ROOT set "ENGINE_DIR=%UE_ROOT%"

rem --- 2) Launcher install registry key ---
if defined ENGINE_DIR goto :engine_found
for /f "tokens=2,*" %%A in ('reg query "HKLM\SOFTWARE\EpicGames\Unreal Engine\%ENGINE_VERSION%" /v InstalledDirectory 2^>nul ^| find /i "InstalledDirectory"') do set "ENGINE_DIR=%%B"

rem --- 3) Default install path ---
if defined ENGINE_DIR goto :engine_found
if exist "C:\Program Files\Epic Games\UE_%ENGINE_VERSION%\Engine" set "ENGINE_DIR=C:\Program Files\Epic Games\UE_%ENGINE_VERSION%"

:engine_found
if not defined ENGINE_DIR goto :err_engine
set "BUILD_BAT=%ENGINE_DIR%\Engine\Build\BatchFiles\Build.bat"
if not exist "%BUILD_BAT%" goto :err_buildbat

if not exist "%LOG_DIR%" mkdir "%LOG_DIR%"

echo [INFO] Engine : %ENGINE_DIR%
echo [INFO] Target : %TARGET% %PLATFORM% %CONFIG%
echo [INFO] Log    : %LOG_FILE%
echo [INFO] Building...

call "%BUILD_BAT%" %TARGET% %PLATFORM% %CONFIG% -Project="%PROJECT_FILE%" -WaitMutex > "%LOG_FILE%" 2>&1
set "BUILD_RC=%ERRORLEVEL%"

rem --- Live Coding blocks external builds while the editor is open ---
findstr /i /c:"Live Coding is active" "%LOG_FILE%" >nul 2>&1
if not errorlevel 1 goto :err_livecoding

if "%BUILD_RC%"=="0" goto :success

echo.
echo ------------------- errors (from log) -------------------
findstr /i /r /c:"error [A-Z]*[0-9][0-9]*" /c:": error" /c:"fatal error" /c:"LNK[0-9][0-9]*" "%LOG_FILE%"
echo ---------------------------------------------------------
echo.
echo ------------------- log tail ----------------------------
powershell -NoProfile -Command "Get-Content -LiteralPath '%LOG_FILE%' -Tail 40"
echo ---------------------------------------------------------
echo [FAIL] Build failed with exit code %BUILD_RC%. Full log: %LOG_FILE%
endlocal & exit /b 1

:success
echo [SUCCESS] %TARGET% %PLATFORM% %CONFIG% build succeeded.
endlocal & exit /b 0

:err_livecoding
echo [LIVECODING] Live Coding is active in a running editor.
echo [LIVECODING] Close the editor, or compile from the editor with Ctrl+Alt+F11.
endlocal & exit /b 3

:err_project
echo [ENV] Project file not found: %PROJECT_FILE%
endlocal & exit /b 2

:err_engine
echo [ENV] Unreal Engine %ENGINE_VERSION% not found.
echo [ENV] Set the UE_ROOT environment variable to the engine root folder.
endlocal & exit /b 2

:err_buildbat
echo [ENV] Build.bat not found: %BUILD_BAT%
echo [ENV] Check that UE_ROOT points to the engine root, not the Engine subfolder.
endlocal & exit /b 2
