@echo off
rem ============================================================
rem  DFO old version vibration - no-loader auto-mount installer
rem  Copies version.dll (proxy) + collector DLL next to DNF.exe.
rem  The proxy is a NEW file (client dir does not ship version.dll),
rem  no original DLL is touched or replaced. Game loads version.dll
rem  at startup -> proxy auto-loads collector -> shm -> SorahkDFO.
rem  Usage: install_proxy.bat [DNF.exe directory]
rem  Default directory: E:\LX (current known game location)
rem ============================================================
setlocal

set "GAME_DIR=%~1"
if "%GAME_DIR%"=="" set "GAME_DIR=E:\LX"

if not exist "%GAME_DIR%\DNF.exe" (
  echo [install] DNF.exe not found in "%GAME_DIR%"
  echo [install] usage: install_proxy.bat ^<DNF.exe directory^>
  exit /b 1
)

copy /y "release\version.dll"        "%GAME_DIR%\version.dll"        >nul
copy /y "release\DfoVibration_OLD.dll" "%GAME_DIR%\DfoVibration_OLD.dll" >nul

if not exist "release\DfoVibration_OLD.dll" (
  echo [install] WARNING: release\DfoVibration_OLD.dll missing - build it first with build_old.bat
)

echo [install] deployed to "%GAME_DIR%":
for %%F in ("%GAME_DIR%\version.dll" "%GAME_DIR%\DfoVibration_OLD.dll") do (
  if exist "%%~F" echo   %%~nxF  %%~zF bytes
)

echo [install] now just start DNF.exe - SorahkDFO.exe will receive events.
endlocal