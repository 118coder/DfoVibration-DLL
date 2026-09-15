@echo off
rem ============================================================
rem DFO OLD vibration plugin - 40JP installer
rem usage: install_40jp.bat [client_dir]   (default E:\Game\Arad40)
rem Copies the 40JP-profile DLL into the client dir UNDER THE NAME
rem DfoVibration_OLD.dll, so the existing host auto_inject (which
rem looks for DfoVibration_OLD.dll) needs NO change.
rem Keep this file ASCII-only (cmd decodes as GBK).
rem ============================================================
setlocal
set GAME=%~1
if "%GAME%"=="" set GAME=E:\Game\Arad40
set SRC=%~dp0release

if not exist "%GAME%\ARAD.exe" (
    echo ERROR: no ARAD.exe at "%GAME%"
    exit /b 1
)

rem Never overwrite a running file / keep a backup of whatever was there
tasklist /fi "imagename eq ARAD.exe" 2>nul | find /i "ARAD.exe" >nul
if not errorlevel 1 (
    echo ERROR: ARAD.exe is running - close the game first.
    exit /b 1
)

if exist "%GAME%\DfoVibration_OLD.dll" (
    copy /y "%GAME%\DfoVibration_OLD.dll" "%GAME%\DfoVibration_OLD.dll.prebak" >nul
)

copy /y "%SRC%\DfoVibration_40JP.dll" "%GAME%\DfoVibration_OLD.dll" >nul
if errorlevel 1 goto :fail

echo ============================================
echo Installed 40JP profile:
echo   %GAME%\DfoVibration_OLD.dll   (40JP build)
echo.
echo Previous dll backed up as DfoVibration_OLD.dll.prebak
echo Log: %GAME%\DfoVibration_OLD_dll.log
echo   Expect: "[hook] target=40JP anchors=14"
echo           "[hook] idx=0 OK 0x00432EA0 len=6"
echo           "[hook] idx=1 OK 0x00426820 len=6"
echo           "[hook] idx=13 OK 0x004D4D40 len=6"
echo ============================================
exit /b 0

:fail
echo INSTALL FAILED
exit /b 1
