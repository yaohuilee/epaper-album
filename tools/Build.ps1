param([string]$IdfPath, [string]$ToolsPath, [string]$PythonDirectory)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'Enter-Idf.ps1') @PSBoundParameters
$projectRoot = Split-Path $PSScriptRoot -Parent
Push-Location -LiteralPath $projectRoot
try {
    idf.py -DIDF_TARGET=esp32s3 build
    if ($LASTEXITCODE -ne 0) { throw 'Firmware build failed.' }
} finally {
    Pop-Location
}
