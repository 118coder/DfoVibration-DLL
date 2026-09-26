@echo off
rem ============================================================
rem DFO OLD vibration plugin - 60US profile installer
rem usage: install_60us.bat [60us_client_dir]
rem Copies the 60US-profile DLL into the client dir UNDER THE NAME
rem DfoVibration_OLD.dll, so the existing host auto_inject (which
rem looks for DfoVibration_OLD.dll) needs NO change.
rem Keep this file ASCII-only (cmd decodes as GBK).
rem ============================================================
setlocal
set GAME=%~1
if "%GAME%"=="" (
    echo usage: install_60us.bat [60us_client_dir]
    echo   e.g. install_60us.bat "E:\Game\DFO"
    exit /b 1
)
set SRC=%~dp0release

if not exist "%GAME%\DFO.exe" if not exist "%GAME%\DFO_fixed_v3.exe" (
    echo ERROR: no DFO.exe / DFO_fixed_v3.exe at "%GAME%"
    exit /b 1
)

rem Back up any existing vibration dll before overwrite (never destroy a running build)
if exist "%GAME%\DfoVibration_OLD.dll" (
    copy /y "%GAME%\DfoVibration_OLD.dll" "%GAME%\DfoVibration_OLD.dll.pre60us" >nul
)

copy /y "%SRC%\DfoVibration_60US.dll" "%GAME%\DfoVibration_OLD.dll" >nul
if errorlevel 1 goto :fail
if exist "%SRC%\DfoVibration_OLD_Loader.exe" copy /y "%SRC%\DfoVibration_OLD_Loader.exe" "%GAME%\" >nul

echo ============================================
echo Installed 60US profile:
echo   %GAME%\DfoVibration_OLD.dll   (60US build)
echo.
echo Existing dll backed up as DfoVibration_OLD.dll.pre60us
echo Log: DfoVibration_OLD_dll.log next to the dll
echo   Expect: "[hook] target=60US ..." + "hooks installed: 3/13 (60US)"
echo   (sender / broadcast / camshake installed; others UNRESOLVED/skip)
echo ============================================
exit /b 0

:fail
echo INSTALL FAILED
exit /b 1
