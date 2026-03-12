@echo off
setlocal EnableExtensions
chcp 65001 >nul
set "VSLANG=1033"

if "%~4"=="" (
    echo Usage: build_user_dsp.cmd ^<source.cpp^> ^<sdk_dir^> ^<output.dll^> ^<output.pdb^>
    exit /b 1
)

set "SOURCE_FILE=%~1"
set "SDK_DIR=%~2"
set "OUTPUT_DLL=%~3"
set "OUTPUT_PDB=%~4"

if not exist "%SOURCE_FILE%" (
    echo Source file not found: "%SOURCE_FILE%"
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

for %%I in ("%OUTPUT_DLL%") do set "OUTPUT_DIR=%%~dpI"
for %%I in ("%OUTPUT_PDB%") do set "PDB_DIR=%%~dpI"
if not exist "%OUTPUT_DIR%" mkdir "%OUTPUT_DIR%"
if not exist "%PDB_DIR%" mkdir "%PDB_DIR%"

for %%I in ("%OUTPUT_DLL%") do set "BUILD_STEM=%%~nI"
set "OBJ_DIR=%OUTPUT_DIR%obj_%BUILD_STEM%"
if not exist "%OBJ_DIR%" mkdir "%OBJ_DIR%"

call "%VSDEVCMD%" -arch=x64 >nul
if errorlevel 1 (
    echo Failed to initialize the MSVC developer environment.
    exit /b 1
)

echo Compiling "%SOURCE_FILE%"
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
    /Fo"%OBJ_DIR%\\" ^
    /Fd"%OBJ_DIR%\vc143.pdb" ^
    /LD "%SOURCE_FILE%" ^
    /link ^
    /NOLOGO ^
    /DLL ^
    /DEBUG ^
    /INCREMENTAL:NO ^
    /OUT:"%OUTPUT_DLL%" ^
    /PDB:"%OUTPUT_PDB%"

exit /b %errorlevel%
