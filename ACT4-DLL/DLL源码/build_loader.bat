@echo off
rem ============================================================
rem DFO OLD Loader build script (32-bit)
rem requires: i686-w64-mingw32-gcc or LLVM-MinGW gcc -m32
rem ============================================================
setlocal
cd /d "%~dp0"

rem --- toolchain discovery ---
set GCC=
if exist "C:\mingw64\mingw64\bin\i686-w64-mingw32-gcc.exe" set GCC=C:\mingw64\mingw64\bin\i686-w64-mingw32-gcc.exe
if not defined GCC if exist "C:\Users\12290\AppData\Local\Microsoft\WinGet\Packages\MartinStorsjo.LLVM-MinGW.UCRT_Microsoft.Winget.Source_8wekyb3d8bbwe\llvm-mingw-20260602-ucrt-x86_64\bin\i686-w64-mingw32-gcc.exe" set GCC=C:\Users\12290\AppData\Local\Microsoft\WinGet\Packages\MartinStorsjo.LLVM-MinGW.UCRT_Microsoft.Winget.Source_8wekyb3d8bbwe\llvm-mingw-20260602-ucrt-x86_64\bin\i686-w64-mingw32-gcc.exe

if not defined GCC (
    echo ERROR: 32-bit gcc not found. Edit build_loader.bat and set GCC manually.
    exit /b 1
)

echo Using: %GCC%
if not exist "release" mkdir "release"
"%GCC%" -O2 -m32 -static -o "release\DfoVibration_OLD_Loader.exe" "loader_main.c" -lws2_32 -static-libgcc
if errorlevel 1 goto :fail

echo ============================================
echo Built: release\DfoVibration_OLD_Loader.exe
echo Usage:
echo   DfoVibration_OLD_Loader.exe                   (watch + inject)
echo   DfoVibration_OLD_Loader.exe ^<DNF.exe path^>   (launcher mode)
echo ============================================
exit /b 0

:fail
echo BUILD FAILED
exit /b 1