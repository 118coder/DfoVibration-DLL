@echo off
rem ============================================================
rem DFO OLD vibration plugin - ACT5 profile installer
rem usage: install_act5.bat [act5_client_dir]
rem Copies the ACT5-profile DLL into the client dir UNDER THE NAME
rem DfoVibration_OLD.dll, so the existing host auto_inject (which
rem looks for DfoVibration_OLD.dll) needs NO change.
rem Keep this file ASCII-only (cmd decodes as GBK).
rem ============================================================
setlocal
set GAME=%~1
if "%GAME%"=="" (
    echo usage: install_act5.bat [act5_client_dir]
    echo   e.g. install_act5.bat "D:\ACT5\Client"
    exit /b 1
)
set SRC=%~dp0release

if not exist "%GAME%\DNFACT5.exe" if not exist "%GAME%\DNF.exe" (
    echo ERROR: no DNFACT5.exe / DNF.exe at "%GAME%"
    exit /b 1
)

rem Back up any existing vibration dll before overwrite (never destroy a running build)
if exist "%GAME%\DfoVibration_OLD.dll" (
    copy /y "%GAME%\DfoVibration_OLD.dll" "%GAME%\DfoVibration_OLD.dll.act1bak" >nul
)

copy /y "%SRC%\DfoVibration_ACT5.dll" "%GAME%\DfoVibration_OLD.dll" >nul
if errorlevel 1 goto :fail
copy /y "%SRC%\DfoVibration_OLD_Loader.exe" "%GAME%\" >nul
if errorlevel 1 goto :fail

echo ============================================
echo Installed ACT5 profile:
echo   %GAME%\DfoVibration_OLD.dll   (ACT5 build)
echo   %GAME%\DfoVibration_OLD_Loader.exe
echo.
echo Existing ACT1 dll backed up as DfoVibration_OLD.dll.act1bak
echo Log: DfoVibration_OLD_dll.log next to the dll
echo   Expect: "[hook] target=ACT5 ..." + "hooks installed: 1/13 (ACT5)"
echo   (only the event sender is located so far; others log UNRESOLVED/skip)
echo ============================================
exit /b 0

:fail
echo INSTALL FAILED
exit /b 1
