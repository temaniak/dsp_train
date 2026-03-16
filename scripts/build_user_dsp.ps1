param(
    [string]$ProjectDir,
    [string]$SdkDir,
    [string]$OutputModule,
    [string]$OutputDebugSymbols
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

if ([string]::IsNullOrWhiteSpace($ProjectDir) -or
    [string]::IsNullOrWhiteSpace($SdkDir) -or
    [string]::IsNullOrWhiteSpace($OutputModule) -or
    [string]::IsNullOrWhiteSpace($OutputDebugSymbols)) {
    Write-Error "Usage: build_user_dsp.ps1 <project_dir> <sdk_dir> <output.module> <debug_symbols_path>"
    exit 1
}

$cmdScript = Join-Path $PSScriptRoot "build_user_dsp.cmd"

if (-not (Test-Path -LiteralPath $cmdScript)) {
    Write-Error "build_user_dsp.cmd was not found next to build_user_dsp.ps1."
    exit 1
}

& $cmdScript $ProjectDir $SdkDir $OutputModule $OutputDebugSymbols
exit $LASTEXITCODE
