@echo off
setlocal

set SCRIPT_DIR=%~dp0
set BUILD_DIR=%SCRIPT_DIR%build

if not "%QT_DIR%"=="" goto :qt_found

for /f "tokens=*" %%i in ('where qmake 2^>nul') do set "QT_BIN_DIR=%%~dpi"
if not defined QT_BIN_DIR (
    echo Error: Cannot find qmake on PATH.
    echo Either add Qt bin to PATH or set QT_DIR, e.g.:
    echo   set QT_DIR=C:\Qt\5.15.2\msvc2019_64
    exit /b 1
)
for %%j in ("%QT_BIN_DIR%..") do set "QT_DIR=%%~fj"

:qt_found
if not "%PJ_INSTALL_DIR%"=="" goto :validate

for /f "tokens=*" %%i in ('where plotjuggler 2^>nul') do set "PJ_BIN_DIR=%%~dpi"
if not defined PJ_BIN_DIR (
    echo Error: Cannot find plotjuggler on PATH.
    echo Either add it to PATH or set PJ_INSTALL_DIR, e.g.:
    echo   set PJ_INSTALL_DIR=C:\path\to\plotjuggler-install
    exit /b 1
)
for %%j in ("%PJ_BIN_DIR%..") do set "PJ_INSTALL_DIR=%%~fj"

:validate
if not exist "%PJ_INSTALL_DIR%\include\PlotJuggler\plotdata.h" (
    echo Error: PJ_INSTALL_DIR=%PJ_INSTALL_DIR% does not contain PlotJuggler headers.
    echo Expected: %PJ_INSTALL_DIR%\include\PlotJuggler\plotdata.h
    exit /b 1
)

echo Using PlotJuggler at: %PJ_INSTALL_DIR%

mkdir "%BUILD_DIR%" 2>nul
cd /d "%BUILD_DIR%"

cmake .. -G "Visual Studio 18 2026" -A x64 -T v142 ^
    -DPJ_INSTALL_DIR="%PJ_INSTALL_DIR%" ^
    -DCMAKE_PREFIX_PATH="%QT_DIR%;%PJ_INSTALL_DIR%" ^
    -DBUILD_TESTING=ON

if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%

cmake --build . --config Release
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%

set JUNIT_ARG=
if not "%JUNIT_OUTPUT%"=="" set "JUNIT_ARG=--output-junit %JUNIT_OUTPUT%"

ctest -C Release --output-on-failure %JUNIT_ARG%
