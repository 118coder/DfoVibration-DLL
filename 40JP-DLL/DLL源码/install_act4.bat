@echo off
rem ============================================================
rem DFO OLD vibration plugin - ACT4 profile installer
rem usage: install_act4.bat [act4_client_dir]
rem Copies the ACT4-profile DLL into the client dir UNDER THE NAME
rem DfoVibration_OLD.dll, so the existing host auto_inject (which
rem looks for DfoVibration_OLD.dll) needs NO change.
rem Keep this file ASCII-only (cmd decodes as GBK).
rem ============================================================
setlocal
set GAME=%~1
if "%GAME%"=="" (
    echo usage: install_act4.bat [act4_client_dir]
    echo   e.g. install_act4.bat "E:\Game\DNF-ACT4\ACT4"
    exit /b 1
)
set SRC=%~dp0release

if not exist "%GAME%\DNF.exe" if not exist "%GAME%\DNFACT4.exe" (
    echo ERROR: no DNF.exe / DNFACT4.exe at "%GAME%"
    exit /b 1
)

rem Back up any existing vibration dll before overwrite (never destroy a running build)
if exist "%GAME%\DfoVibration_OLD.dll" (
    copy /y "%GAME%\DfoVibration_OLD.dll" "%GAME%\DfoVibration_OLD.dll.act1bak" >nul
)

copy /y "%SRC%\DfoVibration_ACT4.dll" "%GAME%\DfoVibration_OLD.dll" >nul
if errorlevel 1 goto :fail

echo ============================================
echo Installed ACT4 profile:
echo   %GAME%\DfoVibration_OLD.dll   (ACT4 build)
echo.
echo Existing dll backed up as DfoVibration_OLD.dll.act1bak
echo Log: DfoVibration_OLD_dll.log next to the dll
echo   Expect: "[hook] target=ACT4 ..." + "hooks installed: 4/13 (ACT4)"
echo ============================================
exit /b 0

:fail
echo INSTALL FAILED
exit /b 1
