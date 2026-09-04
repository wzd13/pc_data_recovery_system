@echo off
setlocal EnableExtensions
cd /d "%~dp0"

where cmake >nul 2>&1
if errorlevel 1 (
  if exist "%ProgramFiles%\CMake\bin\cmake.exe" set "PATH=%ProgramFiles%\CMake\bin;%PATH%"
)

where cmake >nul 2>&1
if errorlevel 1 (
  echo CMake not found. Install Kitware CMake and re-run.
  exit /b 1
)

REM Prefer VS 2022 generator; fall back to default if unavailable
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
if errorlevel 1 (
  echo VS 2022 generator failed — trying default CMake generator...
  cmake -S . -B build -A x64
  if errorlevel 1 exit /b 1
)

cmake --build build --config Release
if errorlevel 1 exit /b 1

if not exist "dist\PCDataRecovery" mkdir "dist\PCDataRecovery"
if not exist "dist\PCDataRecovery\config" mkdir "dist\PCDataRecovery\config"
if not exist "dist\PCDataRecovery\data" mkdir "dist\PCDataRecovery\data"
if not exist "dist\PCDataRecovery\logs" mkdir "dist\PCDataRecovery\logs"

copy /Y "build\Release\PCDataRecovery.exe" "dist\PCDataRecovery\PCDataRecovery.exe" >nul
if exist "build\PCDataRecovery.exe" copy /Y "build\PCDataRecovery.exe" "dist\PCDataRecovery\PCDataRecovery.exe" >nul

echo.
echo Build complete.
echo Portable app: dist\PCDataRecovery\PCDataRecovery.exe
endlocal
