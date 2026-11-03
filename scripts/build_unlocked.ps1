# Unlocks every augment card in the save (Codex: all discovered), then builds
# the game (scripts/build_windows.ps1).
#   -SaveDir <folder>  edit the save in that folder instead of %APPDATA%\Asteria
#   -NoBuild           only edit the save
#   -Rebuild           passed on to build_windows.ps1
# The current save is copied to asteria_save.cfg.before-unlock first.
param([string]$SaveDir = '', [switch]$NoBuild, [switch]$Rebuild)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
if (Get-Process WiNILL -ErrorAction SilentlyContinue) {
    throw 'Close the game first: it rewrites the save on exit.'
}

$dir = if ($SaveDir) { $SaveDir } else { Join-Path $env:APPDATA 'Asteria' }
$save = Join-Path $dir 'asteria_save.cfg'
if (Test-Path $save) {
    Copy-Item $save "$save.before-unlock" -Force
    Write-Host "backup: $save.before-unlock"
}

$installer = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer"
$vswhere = Join-Path $installer 'vswhere.exe'
$env:PATH = "$installer;$env:PATH"
$vs = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $vs) { throw 'Visual Studio C++ tools required' }
$vcvars = Join-Path $vs 'VC\Auxiliary\Build\vcvars64.bat'
$out = Join-Path $root 'build\tools\unlock-all-augments'
New-Item -ItemType Directory -Force $out | Out-Null
Push-Location $root
try {
    $includes = '/I Include /I WiNILL\Core /I WiNILL\Core\Scenes /I WiNILL\Render /I WiNILL\Entity /I WiNILL\System'
    $command = "call `"$vcvars`" >nul && cl /nologo /utf-8 /std:c++17 /EHsc /MT /DNOMINMAX $includes tools\unlock_all_augments.cpp WiNILL\Entity\AugmentCatalog.cpp /Fo`"$out\\`" /Fe`"$out\unlock_all_augments.exe`""
    & cmd.exe /d /s /c $command
    if ($LASTEXITCODE -ne 0) { throw 'unlock tool build failed' }

    $env:ASTERIA_SAVE_DIR = $dir
    try { & "$out\unlock_all_augments.exe" } finally { Remove-Item Env:ASTERIA_SAVE_DIR -ErrorAction SilentlyContinue }
    if ($LASTEXITCODE -ne 0) { throw 'unlock tool failed' }
} finally { Pop-Location }

if (-not $NoBuild) {
    if ($Rebuild) { & (Join-Path $PSScriptRoot 'build_windows.ps1') -Rebuild }
    else          { & (Join-Path $PSScriptRoot 'build_windows.ps1') }
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}
