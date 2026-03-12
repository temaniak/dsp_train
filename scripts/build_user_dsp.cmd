@echo off
setlocal EnableExtensions EnableDelayedExpansion
chcp 65001 >nul
set "VSLANG=1033"

if "%~4"=="" (
    echo Usage: build_user_dsp.cmd ^<project_dir^> ^<sdk_dir^> ^<output.module^> ^<debug_symbols_path^>
    exit /b 1
)

set "PROJECT_DIR=%~1"
set "SDK_DIR=%~2"
set "OUTPUT_MODULE=%~3"
set "OUTPUT_DEBUG_SYMBOLS=%~4"

if not exist "%PROJECT_DIR%" (
    echo Project directory not found: "%PROJECT_DIR%"
    exit /b 1
)

if not exist "%SDK_DIR%\UserDspApi.h" (
    echo User DSP SDK header not found in "%SDK_DIR%"
    exit /b 1
)

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

for %%I in ("%OUTPUT_MODULE%") do set "OUTPUT_DIR=%%~dpI"
for %%I in ("%OUTPUT_DEBUG_SYMBOLS%") do set "PDB_DIR=%%~dpI"
if not exist "%OUTPUT_DIR%" mkdir "%OUTPUT_DIR%"
if not exist "%PDB_DIR%" mkdir "%PDB_DIR%"

for %%I in ("%OUTPUT_MODULE%") do set "BUILD_STEM=%%~nI"
set "OBJ_DIR=%OUTPUT_DIR%obj_%BUILD_STEM%"
if not exist "%OBJ_DIR%" mkdir "%OBJ_DIR%"

call "%VSDEVCMD%" -arch=x64 >nul
if errorlevel 1 (
    echo Failed to initialize the MSVC developer environment.
    exit /b 1
)

set "SOURCE_FILES="
for /r "%PROJECT_DIR%" %%F in (*.cpp) do (
    set "SOURCE_FILES=!SOURCE_FILES! "%%~fF""
)

if not defined SOURCE_FILES (
    echo No .cpp source files were found in "%PROJECT_DIR%"
    exit /b 1
)

set "PROJECT_INCLUDE=%PROJECT_DIR%\include"

echo Compiling project "%PROJECT_DIR%"
cl ^
    /nologo ^
    /std:c++20 ^
    /EHsc ^
    /MD ^
    /utf-8 ^
    /W4 ^
    /Zi ^
    /Od ^
    /I"%SDK_DIR%" ^
    /I"%PROJECT_DIR%" ^
    /I"%PROJECT_INCLUDE%" ^
    /Fo"%OBJ_DIR%\\" ^
    /Fd"%OBJ_DIR%\vc143.pdb" ^
    /LD !SOURCE_FILES! ^
    /link ^
    /NOLOGO ^
    /DLL ^
    /DEBUG ^
    /INCREMENTAL:NO ^
    /OUT:"%OUTPUT_MODULE%" ^
    /PDB:"%OUTPUT_DEBUG_SYMBOLS%"

exit /b %errorlevel%
