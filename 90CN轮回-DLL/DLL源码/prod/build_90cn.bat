@echo off
rem ============================================================
rem 90CN 生产版 build (LLVM-MinGW x86)
rem output:
rem   release\DfoVibration.dll              (生产 DLL, 宿主只认此模块名)
rem   release\DfoVibration_90CN_Loader.exe  (v1.2+, DLL_NAME=DfoVibration.dll)
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

echo [1/2] Building DfoVibration.dll (x86, 90CN production) ...
"%TOOL%\gcc.exe" -m32 -O2 -Wall -Wextra -fms-extensions -o "%OUT%\DfoVibration.dll" "%ROOT%dfovib_90cn.c" "%ROOT%hooks_90cn.c" "%ROOT%hooks_90cn_asm.s" "%ROOT%..\common\ini_util.c" -shared -static-libgcc
if errorlevel 1 goto :fail

echo [2/2] Building DfoVibration_90CN_Loader.exe (x86, v1.2, DLL_NAME=DfoVibration.dll) ...
"%TOOL%\windres.exe" --target=pe-i386 -i "%ROOT%..\probe\loader.rc" -O coff -o "%OUT%\loader.res.o"
if errorlevel 1 goto :fail
"%TOOL%\gcc.exe" -m32 -O2 -Wall -static -o "%OUT%\DfoVibration_90CN_Loader.exe" "%ROOT%..\probe\loader_main.c" "%OUT%\loader.res.o" -static-libgcc
if errorlevel 1 goto :fail

echo.
echo ============ BUILD OK ============
echo output: %OUT%
echo   DfoVibration.dll
echo   DfoVibration_90CN_Loader.exe
exit /b 0

:fail
echo.
echo BUILD FAILED.
exit /b 1
