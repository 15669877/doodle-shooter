@echo off
rem ============================================================
rem  doodle_shooter 一键构建 (Windows)
rem    优先: Visual Studio 的 MSBuild  -> 产出与 IDE 完全一致的工程产物
rem    回退: MinGW gcc / MSVC cl
rem ============================================================
setlocal
cd /d "%~dp0"

rem ---- 1. 尝试 MSBuild (Visual Studio) ----
set "MSB="
for %%P in (
  "%ProgramFiles%\Microsoft Visual Studio\2026\Community\MSBuild\Current\Bin\MSBuild.exe"
  "%ProgramFiles%\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe"
  "%ProgramFiles%\Microsoft Visual Studio\2022\Professional\MSBuild\Current\Bin\MSBuild.exe"
  "%ProgramFiles%\Microsoft Visual Studio\2022\Enterprise\MSBuild\Current\Bin\MSBuild.exe"
  "%ProgramFiles(x86)%\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe"
) do if exist %%P if not defined MSB set "MSB=%%~P"

if defined MSB (
  echo [1/2] 使用 Visual Studio MSBuild 构建 ...
  echo        %MSB%
  "%MSB%" doodle_shooter.sln /p:Configuration=Release /p:Platform=x64 /t:Rebuild /v:minimal /nologo
  if errorlevel 1 goto :fail
  echo [2/2] 运行自检 ...
  x64\Release\doodle_tools.exe --selftest
  if errorlevel 1 goto :fail
  echo.
  echo 构建完成:
  echo   x64\Release\doodle_shooter.exe   (图形窗口版, 直接双击即玩)
  echo   x64\Release\doodle_tools.exe     (命令行工具版)
  echo 也可以双击 doodle_shooter.sln 在 VS 中 F5 运行.
  endlocal
  exit /b 0
)

rem ---- 2. 回退: MinGW ----
where gcc >nul 2>nul
if %ERRORLEVEL%==0 goto :mingw
where cl >nul 2>nul
if %ERRORLEVEL%==0 goto :msvc

echo [ERROR] 未找到 MSBuild / gcc / cl.
echo        请安装 Visual Studio (含 C++ 桌面开发工作负载) 或 MinGW-w64.
endlocal
exit /b 1

:mingw
echo [1/2] 编译 (MinGW gcc) ...
gcc -O2 -Wall -Wextra -std=c99 -mwindows -o game.exe game.c -lgdi32 -luser32 || goto :fail
gcc -O2 -Wall -Wextra -std=c99 -o game_console.exe game.c -lgdi32 -luser32 || goto :fail
echo [2/2] 运行自检 ...
game_console.exe --selftest || goto :fail
echo.
echo 构建完成: game.exe (窗口版) / game_console.exe (工具版)
endlocal
exit /b 0

:msvc
echo [1/2] 编译 (MSVC cl) ...
cl /nologo /TC /W4 /O2 /utf-8 /Fe:game_console.exe game.c /link user32.lib gdi32.lib || goto :fail
cl /nologo /TC /W4 /O2 /utf-8 /Fe:game.exe game.c /link /SUBSYSTEM:WINDOWS user32.lib gdi32.lib || goto :fail
echo [2/2] 运行自检 ...
game_console.exe --selftest || goto :fail
echo.
echo 构建完成: game.exe (窗口版) / game_console.exe (工具版)
endlocal
exit /b 0

:fail
echo.
echo [ERROR] 构建失败
endlocal
exit /b 1
