@echo off
rem ============================================================
rem  DFO old version vibration - version.dll proxy build script
rem  Mount way matches the accepted SimSunFontHook precedent
rem  (E:\LX\A1模拟源码\tools\SimSunFontHook\): ADD a NEW
rem  version.dll next to DNF.exe - never touch any original DLL
rem  or System32. DNF.exe imports version.dll so it loads this
rem  proxy at startup; the proxy auto-loads the collector DLL
rem  and forwards all version APIs to the real System32 dll.
rem  Output : release\version.dll (32-bit, matches DNF.exe arch)
rem  Deploy : version.dll + DfoVibration_OLD.dll next to DNF.exe
rem ============================================================
setlocal

set "GCC=C:\Users\12290\AppData\Local\Microsoft\WinGet\Packages\MartinStorsjo.LLVM-MinGW.UCRT_Microsoft.Winget.Source_8wekyb3d8bbwe\llvm-mingw-20260602-ucrt-x86_64\bin"

if not exist "release" mkdir release

"%GCC%\i686-w64-mingw32-gcc" -shared -O2 -static-libgcc ^
  -o release\version.dll version_proxy.c version.def

if errorlevel 1 (
  echo [build] FAILED
  exit /b 1
)

echo [build] OK: release\version.dll
for %%F in (release\version.dll) do echo [build] size=%%~zF bytes

rem ---- export check (17 names, no @, equal to System32 version.dll) ----
echo [build] exports:
"%GCC%\llvm-readobj.exe" --coff-exports release\version.dll 2>nul | findstr /C:"Name:"

endlocal