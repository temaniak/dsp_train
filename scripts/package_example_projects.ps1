param(
    [string]$SourceRoot = (Join-Path $PSScriptRoot "..\\example_projects"),
    [string]$OutputRoot = (Join-Path $PSScriptRoot "..\\example_project_archives")
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem

$resolvedSourceRoot = (Resolve-Path $SourceRoot).Path

if (-not (Test-Path $OutputRoot)) {
    New-Item -ItemType Directory -Path $OutputRoot | Out-Null
}

$resolvedOutputRoot = (Resolve-Path $OutputRoot).Path
$compressionLevel = [System.IO.Compression.CompressionLevel]::Optimal

$exampleDirectories = Get-ChildItem -Path $resolvedSourceRoot -Directory | Sort-Object Name

function Get-NormalizedRelativePath {
    param(
        [string]$BasePath,
        [string]$TargetPath
    )

    $baseUri = New-Object System.Uri(((Resolve-Path $BasePath).Path.TrimEnd('\') + '\'))
    $targetUri = New-Object System.Uri((Resolve-Path $TargetPath).Path)
    return [System.Uri]::UnescapeDataString($baseUri.MakeRelativeUri($targetUri).ToString())
}

foreach ($exampleDirectory in $exampleDirectories) {
    $projectJsonPath = Join-Path $exampleDirectory.FullName "project.json"
    $filesDirectoryPath = Join-Path $exampleDirectory.FullName "files"

    if (-not (Test-Path $projectJsonPath)) {
        throw "Missing project.json in $($exampleDirectory.FullName)"
    }

    if (-not (Test-Path $filesDirectoryPath)) {
        throw "Missing files directory in $($exampleDirectory.FullName)"
    }

    $archivePath = Join-Path $resolvedOutputRoot ($exampleDirectory.Name + ".dspedu")

    if (Test-Path $archivePath) {
        Remove-Item $archivePath -Force
    }

    $archive = [System.IO.Compression.ZipFile]::Open($archivePath, [System.IO.Compression.ZipArchiveMode]::Create)

    try {
        $files = Get-ChildItem -Path $exampleDirectory.FullName -File -Recurse | Sort-Object FullName

        foreach ($file in $files) {
            $archiveEntryPath = Get-NormalizedRelativePath -BasePath $exampleDirectory.FullName -TargetPath $file.FullName
            [System.IO.Compression.ZipFileExtensions]::CreateEntryFromFile($archive, $file.FullName, $archiveEntryPath, $compressionLevel) | Out-Null
        }
    }
    finally {
        $archive.Dispose()
    }

    Write-Host "Packed $($exampleDirectory.Name) -> $archivePath"
}
