param(
    [string]$IdfPath,
    [string]$ToolsPath,
    [string]$PythonDirectory
)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
$localConfig = Join-Path $projectRoot '.local/idf-env.json'
if (Test-Path -LiteralPath $localConfig) {
    $settings = Get-Content -LiteralPath $localConfig -Raw | ConvertFrom-Json
    if (-not $IdfPath) { $IdfPath = $settings.idf_path }
    if (-not $ToolsPath) { $ToolsPath = $settings.tools_path }
    if (-not $PythonDirectory) { $PythonDirectory = $settings.python_directory }
}
if (-not $IdfPath) { $IdfPath = $env:IDF_PATH }
if (-not $ToolsPath) { $ToolsPath = $env:IDF_TOOLS_PATH }
if (-not $IdfPath -or -not (Test-Path -LiteralPath (Join-Path $IdfPath 'export.ps1'))) {
    throw 'Provide -IdfPath or create .local/idf-env.json; see docs/development.md.'
}
if ($PythonDirectory -and -not $env:IDF_PYTHON_ENV_PATH) { $env:Path = "$PythonDirectory;$env:Path" }
if ($ToolsPath) { $env:IDF_TOOLS_PATH = (Resolve-Path -LiteralPath $ToolsPath).Path }
$env:IDF_PATH = (Resolve-Path -LiteralPath $IdfPath).Path
. (Join-Path $env:IDF_PATH 'export.ps1')
if ($LASTEXITCODE -ne 0) { throw 'ESP-IDF activation failed.' }
idf.py --version
if ($LASTEXITCODE -ne 0) { throw 'idf.py is unavailable.' }
