param(
    [string]$BuildDir = (Join-Path $PSScriptRoot "..\\build-win-package"),
    [string]$Configuration = $(if ($env:DSP_EDU_BUILD_CONFIG) { $env:DSP_EDU_BUILD_CONFIG } else { "Release" }),
    [string]$OutputRoot = (Join-Path $PSScriptRoot "..\\dist"),
    [string]$PackageName = "DSP Education Stand Windows x64",
    [switch]$SkipExampleRefresh
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem

function Resolve-FullPath {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Path,
        [Parameter(Mandatory = $true)]
        [string]$BasePath
    )

    if ([System.IO.Path]::IsPathRooted($Path)) {
        return [System.IO.Path]::GetFullPath($Path)
    }

    return [System.IO.Path]::GetFullPath((Join-Path $BasePath $Path))
}

function Ensure-Directory {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Path
    )

    if (-not (Test-Path -LiteralPath $Path)) {
        New-Item -ItemType Directory -Path $Path -Force | Out-Null
    }
}

function Copy-FileToBundle {
    param(
        [Parameter(Mandatory = $true)]
        [string]$SourcePath,
        [Parameter(Mandatory = $true)]
        [string]$DestinationPath
    )

    Ensure-Directory -Path (Split-Path -Parent $DestinationPath)
    Copy-Item -LiteralPath $SourcePath -Destination $DestinationPath -Force
}

function Copy-DirectoryContentsToBundle {
    param(
        [Parameter(Mandatory = $true)]
        [string]$SourcePath,
        [Parameter(Mandatory = $true)]
        [string]$DestinationPath
    )

    Ensure-Directory -Path $DestinationPath

    Get-ChildItem -LiteralPath $SourcePath -Force | ForEach-Object {
        Copy-Item -LiteralPath $_.FullName -Destination $DestinationPath -Recurse -Force
    }
}

$projectRoot = Resolve-FullPath -Path ".." -BasePath $PSScriptRoot
$resolvedBuildDir = Resolve-FullPath -Path $BuildDir -BasePath $projectRoot
$resolvedOutputRoot = Resolve-FullPath -Path $OutputRoot -BasePath $projectRoot
$bundleRoot = Join-Path $resolvedOutputRoot $PackageName
$zipPath = Join-Path $resolvedOutputRoot ($PackageName + ".zip")
$stagedExampleArchivesPath = Join-Path $resolvedBuildDir "packaged_example_project_archives"

$buildScript = Join-Path $projectRoot "scripts\\build_windows.cmd"
$examplePackageScript = Join-Path $projectRoot "scripts\\package_example_projects.ps1"

& $buildScript $resolvedBuildDir $Configuration
if ($LASTEXITCODE -ne 0) {
    throw "Windows build failed with exit code $LASTEXITCODE."
}

if (-not $SkipExampleRefresh) {
    if (Test-Path -LiteralPath $stagedExampleArchivesPath) {
        Remove-Item -LiteralPath $stagedExampleArchivesPath -Recurse -Force
    }

    & $examplePackageScript `
        -SourceRoot (Join-Path $projectRoot "example_projects") `
        -OutputRoot $stagedExampleArchivesPath

    if ($LASTEXITCODE -ne 0) {
        throw "Refreshing example project archives failed with exit code $LASTEXITCODE."
    }
}

$exePath = Join-Path $resolvedBuildDir "DspEducationStand_artefacts\\$Configuration\\DSP Education Stand.exe"
$iconPath = Join-Path $resolvedBuildDir "DspEducationStand_artefacts\\JuceLibraryCode\\icon.ico"
$packageReadmePath = Join-Path $bundleRoot "README.txt"

$requiredFiles = @(
    $exePath,
    $iconPath,
    (Join-Path $projectRoot "README.md"),
    (Join-Path $projectRoot "manual_en.md"),
    (Join-Path $projectRoot "manual_ru.md"),
    (Join-Path $projectRoot "icon.png"),
    (Join-Path $projectRoot "scripts\\build_user_dsp.cmd"),
    (Join-Path $projectRoot "scripts\\build_user_dsp.ps1"),
    (Join-Path $projectRoot "user_dsp_sdk\\UserDspApi.h")
)

foreach ($path in $requiredFiles) {
    if (-not (Test-Path -LiteralPath $path)) {
        throw "Required package input was not found: $path"
    }
}

$exampleArchivesPath = if ($SkipExampleRefresh) {
    Join-Path $projectRoot "example_project_archives"
} else {
    $stagedExampleArchivesPath
}

if (-not (Test-Path -LiteralPath $exampleArchivesPath)) {
    throw "Example archive directory was not found: $exampleArchivesPath"
}

Ensure-Directory -Path $resolvedOutputRoot

if (Test-Path -LiteralPath $bundleRoot) {
    Remove-Item -LiteralPath $bundleRoot -Recurse -Force
}

if (Test-Path -LiteralPath $zipPath) {
    Remove-Item -LiteralPath $zipPath -Force
}

Ensure-Directory -Path $bundleRoot
Ensure-Directory -Path (Join-Path $bundleRoot "scripts")
Ensure-Directory -Path (Join-Path $bundleRoot "user_dsp_sdk")
Ensure-Directory -Path (Join-Path $bundleRoot "example_project_archives")

Copy-FileToBundle -SourcePath $exePath -DestinationPath (Join-Path $bundleRoot "DSP Education Stand.exe")
Copy-FileToBundle -SourcePath $iconPath -DestinationPath (Join-Path $bundleRoot "app-icon.ico")
Copy-FileToBundle -SourcePath (Join-Path $projectRoot "icon.png") -DestinationPath (Join-Path $bundleRoot "icon.png")
Copy-FileToBundle -SourcePath (Join-Path $projectRoot "README.md") -DestinationPath (Join-Path $bundleRoot "README.md")
Copy-FileToBundle -SourcePath (Join-Path $projectRoot "manual_en.md") -DestinationPath (Join-Path $bundleRoot "manual_en.md")
Copy-FileToBundle -SourcePath (Join-Path $projectRoot "manual_ru.md") -DestinationPath (Join-Path $bundleRoot "manual_ru.md")
Copy-FileToBundle -SourcePath (Join-Path $projectRoot "scripts\\build_user_dsp.cmd") -DestinationPath (Join-Path $bundleRoot "scripts\\build_user_dsp.cmd")
Copy-FileToBundle -SourcePath (Join-Path $projectRoot "scripts\\build_user_dsp.ps1") -DestinationPath (Join-Path $bundleRoot "scripts\\build_user_dsp.ps1")
Copy-FileToBundle -SourcePath (Join-Path $projectRoot "user_dsp_sdk\\UserDspApi.h") -DestinationPath (Join-Path $bundleRoot "user_dsp_sdk\\UserDspApi.h")

Copy-DirectoryContentsToBundle -SourcePath $exampleArchivesPath -DestinationPath (Join-Path $bundleRoot "example_project_archives")

$packageReadme = @"
DSP Education Stand Windows x64

Start:
- Run "DSP Education Stand.exe".

Included:
- example_project_archives\*.dspedu educational projects
- manual_en.md and manual_ru.md
- scripts\build_user_dsp.cmd, scripts\build_user_dsp.ps1 and user_dsp_sdk\UserDspApi.h for in-app user DSP builds
- app-icon.ico and icon.png generated from the packaged application icon

Important:
- The package is portable and can be moved as a folder or extracted from the zip archive.
- User DSP compilation on Windows still requires Visual Studio Build Tools with C++ support.
- The application stores its workspace and settings under %LOCALAPPDATA%\DSP Education Stand.
"@

Set-Content -LiteralPath $packageReadmePath -Value $packageReadme.Trim() -Encoding utf8

[System.IO.Compression.ZipFile]::CreateFromDirectory($bundleRoot, $zipPath, [System.IO.Compression.CompressionLevel]::Optimal, $true)

Write-Host "Created portable bundle: $bundleRoot"
Write-Host "Created zip archive: $zipPath"
