@echo off
rem ---------------------------------------------------------------
rem  MSVC manual build (same flags as the VS project)
rem  Does not depend on vcvars64.bat; SDK paths are set explicitly.
rem  .lib files must come after /link so cl treats them as linker input.
rem ---------------------------------------------------------------
setlocal
set "VSDIR=D:/Program Files\Microsoft Visual Studio\18\Community"
set "MSVCDIR=%VSDIR%\VC\Tools\MSVC\14.51.36231"
set "SDKDIR=D:/Windows Kits\10"
set "SDKVER=10.0.26100.0"
set "INCLUDE=%MSVCDIR%\include;%SDKDIR%\Include\%SDKVER%\ucrt;%SDKDIR%\Include\%SDKVER%\um;%SDKDIR%\Include\%SDKVER%\shared;%SDKDIR%\Include\%SDKVER%\winrt"
set "LIB=%MSVCDIR%\lib\x64;%SDKDIR%\Lib\%SDKVER%\ucrt\x64;%SDKDIR%\Lib\%SDKVER%\um\x64"
set "PATH=%MSVCDIR%\bin\Hostx64\x64;%SDKDIR%\bin\%SDKVER%\x64;%PATH%"
cd /d "%~dp0"

echo === [1/3] MSVC /TC /W4 /O2 /utf-8 console ===
cl /nologo /TC /W4 /O2 /utf-8 /Fe:msvc_console.exe game.c /link user32.lib gdi32.lib
if errorlevel 1 goto fail

echo.
echo === [2/3] MSVC /TC /W4 /O2 /utf-8 window ===
cl /nologo /TC /W4 /O2 /utf-8 /Fe:msvc_game.exe game.c /link /SUBSYSTEM:WINDOWS user32.lib gdi32.lib
if errorlevel 1 goto fail

echo.
echo === [3/3] selftest ===
msvc_console.exe --selftest
if errorlevel 1 goto fail
msvc_console.exe --sim
if errorlevel 1 goto fail

echo.
echo BUILD OK: msvc_game.exe (window) / msvc_console.exe (tools)
endlocal
exit /b 0

:fail
echo.
echo BUILD FAILED
endlocal
exit /b 1
