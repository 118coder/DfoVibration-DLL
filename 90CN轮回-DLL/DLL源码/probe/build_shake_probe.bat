@echo off
rem ============================================================
rem 90CN shake probe v1.4 build (LLVM-MinGW x86)
rem output: release\DfoVibration_shake_probe.dll
rem deploy: 复制到 E:\Game\90CN 并改名 DfoVibration.dll
rem         (宿主/Loader 只认 DfoVibration.dll; 实测完换回生产版)
rem temp/cache redirected to work\cache (keep C: clean)
rem ============================================================
setlocal
set TOOL=C:\Users\12290\AppData\Local\Microsoft\WinGet\Packages\MartinStorsjo.LLVM-MinGW.UCRT_Microsoft.Winget.Source_8wekyb3d8bbwe\llvm-mingw-20260602-ucrt-x86_64\bin
set ROOT=%~dp0
set OUT=%ROOT%..\release
set CACHE=%ROOT%..\..\work\cache
if not exist "%OUT%" mkdir "%OUT%"
if not exist "%CACHE%" mkdir "%CACHE%"

for %%I in ("%CACHE%") do set CACHE_S=%%~sI
if "%CACHE_S%"=="" set CACHE_S=%CACHE%
set TMP=%CACHE_S%
set TEMP=%CACHE_S%

echo [1/1] Building DfoVibration_shake_probe.dll (x86, v1.4) ...
"%TOOL%\gcc.exe" -m32 -O2 -Wall -Wextra -fms-extensions -o "%OUT%\DfoVibration_shake_probe.dll" "%ROOT%shake_probe.c" "%ROOT%shake_stubs.s" -shared -static-libgcc
if errorlevel 1 goto :fail

echo.
echo ============ BUILD OK ============
echo output: %OUT%\DfoVibration_shake_probe.dll
echo deploy: copy to game dir as DfoVibration.dll
exit /b 0

:fail
echo.
echo BUILD FAILED.
exit /b 1
