@echo off
setlocal EnableExtensions

set "SCRIPT_DIR=%~dp0"
for %%I in ("%SCRIPT_DIR%..") do set "PROJECT_ROOT=%%~fI"

set "BUILD_DIR=%PROJECT_ROOT%\build"
if not "%~1"=="" set "BUILD_DIR=%~1"
set "BUILD_CONFIG=Release"
if defined DSP_EDU_BUILD_CONFIG set "BUILD_CONFIG=%DSP_EDU_BUILD_CONFIG%"
if not "%~2"=="" set "BUILD_CONFIG=%~2"

set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" (
    echo vswhere.exe was not found at "%VSWHERE%".
    exit /b 1
)

set "VSDEVCMD="
for /f "usebackq delims=" %%I in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -find Common7\Tools\VsDevCmd.bat`) do (
    set "VSDEVCMD=%%I"
)

if not defined VSDEVCMD (
    echo VsDevCmd.bat was not found. Install Visual Studio Build Tools with C++ support.
    exit /b 1
)

call "%VSDEVCMD%" -arch=x64 >nul
if errorlevel 1 (
    echo Failed to initialize the MSVC developer environment.
    exit /b 1
)

if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"

set "JUCE_PATH=C:\Users\User\JUCE"
if defined JUCE_DIR set "JUCE_PATH=%JUCE_DIR%"
set "JUCE_ARG=-DJUCE_DIR:PATH=%JUCE_PATH%"

cmake -S "%PROJECT_ROOT%" -B "%BUILD_DIR%" -G Ninja %JUCE_ARG% -DCMAKE_BUILD_TYPE=%BUILD_CONFIG%
exit /b %errorlevel%
