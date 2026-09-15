@echo off
rem ============================================================
rem DFO OLD vibration plugin build script (32-bit DLL)
rem requires: i686-w64-mingw32-gcc (LLVM-MinGW or mingw-w64)
rem ============================================================
setlocal
cd /d "%~dp0"

rem --- toolchain discovery ---
set GCC=
if exist "C:\mingw64\mingw64\bin\i686-w64-mingw32-gcc.exe" set GCC=C:\mingw64\mingw64\bin\i686-w64-mingw32-gcc.exe
if not defined GCC if exist "C:\Users\12290\AppData\Local\Microsoft\WinGet\Packages\MartinStorsjo.LLVM-MinGW.UCRT_Microsoft.Winget.Source_8wekyb3d8bbwe\llvm-mingw-20260602-ucrt-x86_64\bin\i686-w64-mingw32-gcc.exe" set GCC=C:\Users\12290\AppData\Local\Microsoft\WinGet\Packages\MartinStorsjo.LLVM-MinGW.UCRT_Microsoft.Winget.Source_8wekyb3d8bbwe\llvm-mingw-20260602-ucrt-x86_64\bin\i686-w64-mingw32-gcc.exe

if not defined GCC (
    echo ERROR: 32-bit gcc not found. Edit build_old.bat and set GCC manually.
    exit /b 1
)

echo Using: %GCC%
if not exist "release" mkdir "release"

rem ---- 1. assemble stubs ----
"%GCC%" -c -O2 hooks_old_asm.s -o build_asm.o
if errorlevel 1 goto :fail

rem ---- 2. compile C sources ----
"%GCC%" -c -O2 -m32 -DWINVER=0x0501 -D_WIN32_WINNT=0x0501 dfovib_old.c -o build_dll.o
if errorlevel 1 goto :fail
"%GCC%" -c -O2 -m32 hooks_old.c -o build_hooks.o
if errorlevel 1 goto :fail
"%GCC%" -c -O2 -m32 common\ini_util.c -o build_ini.o
if errorlevel 1 goto :fail

rem ---- 3. link DLL ----
"%GCC%" -m32 -shared -static -o "release\DfoVibration_OLD.dll" build_dll.o build_hooks.o build_ini.o build_asm.o -static-libgcc -Wl,--out-implib,release\DfoVibration_OLD.lib
if errorlevel 1 goto :fail

del /q build_asm.o build_dll.o build_hooks.o build_ini.o 2>nul

echo ============================================
echo Built: release\DfoVibration_OLD.dll
echo Place next to loader, then run:
echo   release\DfoVibration_OLD_Loader.exe
echo ============================================
exit /b 0

:fail
echo BUILD FAILED
exit /b 1