@echo off
rem ============================================================
rem DFO OLD vibration plugin - ACT4 profile build (32-bit DLL)
rem Same toolchain as build_old.bat, plus -DVIB_TARGET=4.
rem Output: release\DfoVibration_ACT4.dll
rem NOTE: keep this file ASCII-only (cmd decodes as GBK).
rem ============================================================
setlocal
cd /d "%~dp0"

rem --- toolchain discovery ---
set GCC=
if exist "C:\mingw64\mingw64\bin\i686-w64-mingw32-gcc.exe" set GCC=C:\mingw64\mingw64\bin\i686-w64-mingw32-gcc.exe
if not defined GCC if exist "C:\Users\12290\AppData\Local\Microsoft\WinGet\Packages\MartinStorsjo.LLVM-MinGW.UCRT_Microsoft.Winget.Source_8wekyb3d8bbwe\llvm-mingw-20260602-ucrt-x86_64\bin\i686-w64-mingw32-gcc.exe" set GCC=C:\Users\12290\AppData\Local\Microsoft\WinGet\Packages\MartinStorsjo.LLVM-MinGW.UCRT_Microsoft.Winget.Source_8wekyb3d8bbwe\llvm-mingw-20260602-ucrt-x86_64\bin\i686-w64-mingw32-gcc.exe

if not defined GCC (
    echo ERROR: 32-bit gcc not found. Edit build_act4.bat and set GCC manually.
    exit /b 1
)

echo Using: %GCC%
if not exist "release" mkdir "release"

set DEFS=-DVIB_TARGET=4 -DWINVER=0x0501 -D_WIN32_WINNT=0x0501

rem ---- 1. assemble stubs ----
"%GCC%" -c -O2 hooks_old_asm.s -o build4_asm.o
if errorlevel 1 goto :fail

rem ---- 2. compile C sources (ACT4 profile) ----
"%GCC%" -c -O2 -m32 %DEFS% dfovib_old.c -o build4_dll.o
if errorlevel 1 goto :fail
"%GCC%" -c -O2 -m32 %DEFS% hooks_old.c -o build4_hooks.o
if errorlevel 1 goto :fail
"%GCC%" -c -O2 -m32 common\ini_util.c -o build4_ini.o
if errorlevel 1 goto :fail

rem ---- 3. link DLL ----
"%GCC%" -m32 -shared -static -o "release\DfoVibration_ACT4.dll" build4_dll.o build4_hooks.o build4_ini.o build4_asm.o -static-libgcc -Wl,--out-implib,release\DfoVibration_ACT4.lib
if errorlevel 1 goto :fail

del /q build4_asm.o build4_dll.o build4_hooks.o build4_ini.o 2>nul

echo ============================================
echo Built: release\DfoVibration_ACT4.dll
echo ============================================
exit /b 0

:fail
echo BUILD FAILED
exit /b 1
