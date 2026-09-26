@echo off
rem ============================================================
rem DFO OLD vibration plugin - 60US profile build (32-bit DLL)
rem Same toolchain as build_old.bat, plus -DVIB_TARGET=60.
rem Output: release\DfoVibration_60US.dll
rem Target client: DFO.exe (Themida) / DFO_fixed_v3.exe (unpacked,
rem identical VA layout) - 2010-07-15 US client at E:\Game\DFO
rem NOTE: keep this file ASCII-only (cmd decodes as GBK).
rem ============================================================
setlocal
cd /d "%~dp0"

rem --- toolchain discovery ---
set GCC=
if exist "C:\mingw64\mingw64\bin\i686-w64-mingw32-gcc.exe" set GCC=C:\mingw64\mingw64\bin\i686-w64-mingw32-gcc.exe
if not defined GCC if exist "C:\Users\12290\AppData\Local\Microsoft\WinGet\Packages\MartinStorsjo.LLVM-MinGW.UCRT_Microsoft.Winget.Source_8wekyb3d8bbwe\llvm-mingw-20260602-ucrt-x86_64\bin\i686-w64-mingw32-gcc.exe" set GCC=C:\Users\12290\AppData\Local\Microsoft\WinGet\Packages\MartinStorsjo.LLVM-MinGW.UCRT_Microsoft.Winget.Source_8wekyb3d8bbwe\llvm-mingw-20260602-ucrt-x86_64\bin\i686-w64-mingw32-gcc.exe

if not defined GCC (
    echo ERROR: 32-bit gcc not found. Edit build_60us.bat and set GCC manually.
    exit /b 1
)

echo Using: %GCC%
if not exist "release" mkdir "release"

set DEFS=-DVIB_TARGET=60 -DWINVER=0x0501 -D_WIN32_WINNT=0x0501

rem ---- 1. assemble stubs ----
"%GCC%" -c -O2 hooks_old_asm.s -o build60_asm.o
if errorlevel 1 goto :fail

rem ---- 2. compile C sources (60US profile) ----
"%GCC%" -c -O2 -m32 %DEFS% dfovib_old.c -o build60_dll.o
if errorlevel 1 goto :fail
"%GCC%" -c -O2 -m32 %DEFS% hooks_old.c -o build60_hooks.o
if errorlevel 1 goto :fail
"%GCC%" -c -O2 -m32 %DEFS% common\ini_util.c -o build60_ini.o
if errorlevel 1 goto :fail

rem ---- 3. link DLL ----
"%GCC%" -m32 -shared -static -o "release\DfoVibration_60US.dll" build60_dll.o build60_hooks.o build60_ini.o build60_asm.o -static-libgcc -Wl,--out-implib,release\DfoVibration_60US.lib
if errorlevel 1 goto :fail

del /q build60_asm.o build60_dll.o build60_hooks.o build60_ini.o 2>nul

echo ============================================
echo Built: release\DfoVibration_60US.dll
echo ============================================
exit /b 0

:fail
echo BUILD FAILED
exit /b 1
