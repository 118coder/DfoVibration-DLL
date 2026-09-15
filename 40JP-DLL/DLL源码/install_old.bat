@echo off
rem ============================================================
rem DFO OLD vibration plugin - installer
rem usage: install_old.bat [game_client_dir]
rem default: F:\A1\LMA1S-release\Client
rem copies loader + dll into the client dir
rem ============================================================
setlocal
set GAME=%~1
if "%GAME%"=="" set GAME=F:\A1\LMA1S-release\Client
set SRC=%~dp0release

if not exist "%GAME%\DNF.exe" (
    echo ERROR: game not found at "%GAME%"
    exit /b 1
)

copy /y "%SRC%\DfoVibration_OLD.dll" "%GAME%\" >nul
if errorlevel 1 goto :fail
copy /y "%SRC%\DfoVibration_OLD_Loader.exe" "%GAME%\" >nul
if errorlevel 1 goto :fail

echo ============================================
echo Installed:
echo   %GAME%\DfoVibration_OLD.dll
echo   %GAME%\DfoVibration_OLD_Loader.exe
echo.
echo Usage:
echo   1. double-click DfoVibration_OLD_Loader.exe (watch mode)
echo      - it waits for DNF.exe and auto-injects the dll
echo   2. or launcher mode:  DfoVibration_OLD_Loader.exe "%GAME%\DNF.exe"
echo   3. logs: DfoVibration_OLD_dll.log next to the dll
echo ============================================
exit /b 0

:fail
echo INSTALL FAILED
exit /b 1