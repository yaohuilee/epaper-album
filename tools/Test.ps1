param([string]$IdfPath, [string]$VsDevCmd)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
if (-not $IdfPath) {
    $settingsPath = Join-Path $projectRoot '.local/idf-env.json'
    if (Test-Path -LiteralPath $settingsPath) {
        $IdfPath = (Get-Content -LiteralPath $settingsPath -Raw | ConvertFrom-Json).idf_path
    } else { $IdfPath = $env:IDF_PATH }
}
$cjson = Join-Path $IdfPath 'components/json/cJSON'
if (-not (Test-Path -LiteralPath (Join-Path $cjson 'cJSON.c'))) { throw 'Provide an ESP-IDF v5.5.3 path for its cJSON source.' }
if (-not $VsDevCmd) {
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
    if (-not (Test-Path -LiteralPath $vswhere)) { throw 'Install Visual Studio C++ Build Tools or supply -VsDevCmd.' }
    $install = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if (-not $install) { throw 'Visual Studio C++ compiler not found.' }
    $VsDevCmd = Join-Path $install 'Common7/Tools/VsDevCmd.bat'
}
if (-not (Test-Path -LiteralPath $VsDevCmd)) { throw 'VsDevCmd.bat not found.' }
$localDir = Join-Path $projectRoot '.local'
New-Item -ItemType Directory -Force -Path $localDir | Out-Null
$runner = Join-Path $localDir 'run-tests.cmd'
$common = '/nologo /W4 /WX /std:c11 /D_CRT_SECURE_NO_WARNINGS /I components/core/include'
$album = "/I components/album/include /I`"$cjson`" components/core/bmp.c components/album/config.c components/album/config_io.c components/album/catalog.c components/album/player.c components/album/files.c `"$cjson/cJSON.c`""
$commands = @(
    '@echo off',
    "call `"$VsDevCmd`" -arch=x64 -host_arch=x64 >nul",
    'if errorlevel 1 exit /b 1',
    "cl $common tests/core_test.c components/core/core.c /Fe:.local/core_test.exe /Fo:.local/",
    'if errorlevel 1 exit /b 1', '.local\core_test.exe', 'if errorlevel 1 exit /b 1',
    "cl $common $album tests/album_test.c /Fe:.local/album_test.exe /Fo:.local/",
    'if errorlevel 1 exit /b 1', '.local\album_test.exe', 'if errorlevel 1 exit /b 1',
    "cl $common /I tests/mocks /I components/services/include $album tests/runtime_test.c /Fe:.local/runtime_test.exe /Fo:.local/",
    'if errorlevel 1 exit /b 1', '.local\runtime_test.exe', 'if errorlevel 1 exit /b 1',
    "cl $common /I components/services tests/provision_test.c components/services/provision_render.c components/services/qrcodegen.c /Fe:.local/provision_test.exe /Fo:.local/",
    'if errorlevel 1 exit /b 1', '.local\provision_test.exe', 'exit /b %errorlevel%'
)
Set-Content -LiteralPath $runner -Value $commands -Encoding ascii
Push-Location -LiteralPath $projectRoot
try {
    & $env:ComSpec /d /c "`"$runner`""
    if ($LASTEXITCODE -ne 0) { throw 'Host tests failed.' }
} finally { Pop-Location }
