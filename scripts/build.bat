@echo off
setlocal enabledelayedexpansion

set "ROOT_DIR=%~dp0.."
set "BUILD_DIR=%ROOT_DIR%\build"
set "GENERATOR=Visual Studio 17 2022"

set "COMMAND=%~1"
set "CONFIG=%~2"

if "%COMMAND%"=="" goto :usage

if /i "%COMMAND%"=="configure" goto :configure
if /i "%COMMAND%"=="build"     goto :build
if /i "%COMMAND%"=="clean"     goto :clean
if /i "%COMMAND%"=="rebuild"   goto :rebuild
goto :usage

:configure
echo [ShowcaseEngine] Configuring with %GENERATOR%...
cmake -B "%BUILD_DIR%" -G "%GENERATOR%" "%ROOT_DIR%"
if %errorlevel% neq 0 (
    echo [ShowcaseEngine] Configure FAILED
    exit /b %errorlevel%
)
echo [ShowcaseEngine] Configure OK
exit /b 0

:build
if "%CONFIG%"=="" (
    echo [ShowcaseEngine] Error: config required. Usage: build.bat build Debug^|Release
    exit /b 1
)
echo [ShowcaseEngine] Building %CONFIG%...
cmake --build "%BUILD_DIR%" --config %CONFIG%
if %errorlevel% neq 0 (
    echo [ShowcaseEngine] Build FAILED
    exit /b %errorlevel%
)
echo [ShowcaseEngine] Build OK (%CONFIG%)
exit /b 0

:clean
if "%CONFIG%"=="" (
    echo [ShowcaseEngine] Error: config required. Usage: build.bat clean Debug^|Release
    exit /b 1
)
echo [ShowcaseEngine] Cleaning %CONFIG%...
cmake --build "%BUILD_DIR%" --config %CONFIG% --target clean
if %errorlevel% neq 0 (
    echo [ShowcaseEngine] Clean FAILED
    exit /b %errorlevel%
)
echo [ShowcaseEngine] Clean OK
exit /b 0

:rebuild
if "%CONFIG%"=="" (
    echo [ShowcaseEngine] Error: config required. Usage: build.bat rebuild Debug^|Release
    exit /b 1
)
echo [ShowcaseEngine] Clean Rebuild %CONFIG%...
call :clean
if %errorlevel% neq 0 exit /b %errorlevel%
call :configure
if %errorlevel% neq 0 exit /b %errorlevel%
call :build
if %errorlevel% neq 0 exit /b %errorlevel%
exit /b 0

:usage
echo Usage: build.bat ^<command^> [config]
echo.
echo Commands:
echo   configure            Generate CMake build files
echo   build ^<config^>       Build the project (Debug^|Release)
echo   clean ^<config^>       Clean build artifacts
echo   rebuild ^<config^>     Clean + Configure + Build
exit /b 1
