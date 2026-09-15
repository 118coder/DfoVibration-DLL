@echo off
rem ============================================================
rem DFO OLD vibration plugin - 40JP DIAGNOSTIC PROBE build
rem Adds -DVIB_PROBE_ENABLE=1 -> compiles vib_probe_table.h (24 candidate hooks)
rem plus [pos]/[posB]/[posi] coordinate probes and [hp] semantics probe.
rem Output: release\DfoVibration_40JP_probe.dll
rem NOTE: diagnostic only - do NOT ship as a stable release.
rem Keep this file ASCII-only (cmd decodes as GBK).
rem ============================================================
setlocal
cd /d "%~dp0"

set GCC=
if exist "C:\mingw64\mingw64\bin\i686-w64-mingw32-gcc.exe" set GCC=C:\mingw64\mingw64\bin\i686-w64-mingw32-gcc.exe
if not defined GCC if exist "C:\Users\12290\AppData\Local\Microsoft\WinGet\Packages\MartinStorsjo.LLVM-MinGW.UCRT_Microsoft.Winget.Source_8wekyb3d8bbwe\llvm-mingw-20260602-ucrt-x86_64\bin\i686-w64-mingw32-gcc.exe" set GCC=C:\Users\12290\AppData\Local\Microsoft\WinGet\Packages\MartinStorsjo.LLVM-MinGW.UCRT_Microsoft.Winget.Source_8wekyb3d8bbwe\llvm-mingw-20260602-ucrt-x86_64\bin\i686-w64-mingw32-gcc.exe

if not defined GCC (
    echo ERROR: 32-bit gcc not found. Edit build_40jp_probe.bat and set GCC manually.
    exit /b 1
)

echo Using: %GCC%
if not exist "release" mkdir "release"

set DEFS=-DVIB_TARGET=40 -DVIB_PROBE_ENABLE=1 -DWINVER=0x0501 -D_WIN32_WINNT=0x0501

"%GCC%" -c -O2 hooks_old_asm.s -o buildp_asm.o
if errorlevel 1 goto :fail
"%GCC%" -c -O2 -m32 %DEFS% dfovib_old.c -o buildp_dll.o
if errorlevel 1 goto :fail
"%GCC%" -c -O2 -m32 %DEFS% hooks_old.c -o buildp_hooks.o
if errorlevel 1 goto :fail
"%GCC%" -c -O2 -m32 %DEFS% common\ini_util.c -o buildp_ini.o
if errorlevel 1 goto :fail

"%GCC%" -m32 -shared -static -o "release\DfoVibration_40JP_probe.dll" buildp_dll.o buildp_hooks.o buildp_ini.o buildp_asm.o -static-libgcc -Wl,--out-implib,release\DfoVibration_40JP_probe.lib
if errorlevel 1 goto :fail

del /q buildp_asm.o buildp_dll.o buildp_hooks.o buildp_ini.o 2>nul

echo ============================================
echo Built: release\DfoVibration_40JP_probe.dll
echo ============================================
exit /b 0

:fail
echo BUILD FAILED
exit /b 1
