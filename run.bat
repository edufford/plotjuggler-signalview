@echo off
setlocal

set SCRIPT_DIR=%~dp0
set BUILD_DIR=%SCRIPT_DIR%build

if not exist "%BUILD_DIR%\Release\SignalViewPlugin.dll" (
    echo Error: Plugin not built. Run build.bat first.
    exit /b 1
)

if not "%PJ_INSTALL_DIR%"=="" goto :find_bin

for /f "tokens=*" %%i in ('where plotjuggler 2^>nul') do set "PJ_BIN=%%i"
if not defined PJ_BIN (
    echo Error: Cannot find plotjuggler.
    echo Either add it to PATH or set PJ_INSTALL_DIR, e.g.:
    echo   set PJ_INSTALL_DIR=C:\path\to\plotjuggler-install
    exit /b 1
)
goto :run

:find_bin
set "PJ_BIN=%PJ_INSTALL_DIR%\bin\plotjuggler.exe"

:run
if not exist "%PJ_BIN%" (
    echo Error: Cannot find plotjuggler at %PJ_BIN%
    exit /b 1
)

"%PJ_BIN%" --plugin_folders "%BUILD_DIR%\Release" -n %*
