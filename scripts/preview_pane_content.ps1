# Paints the boss panes' memo / code / image contents to BMP files.
# Usage: preview_pane_content.ps1 [outDir]   (default build\tests\pane-content)
param([string]$OutDir = '')
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$installer = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer"
$env:PATH = "$installer;$env:PATH"
$vs = & (Join-Path $installer 'vswhere.exe') -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $vs) { throw 'Visual Studio C++ tools required' }
$vcvars = Join-Path $vs 'VC\Auxiliary\Build\vcvars64.bat'
$out = Join-Path $root 'build\tests\pane-content'
if (-not $OutDir) { $OutDir = $out }
New-Item -ItemType Directory -Force $out, $OutDir | Out-Null
Push-Location $root
try {
    $command = "call `"$vcvars`" >nul && cl /nologo /utf-8 /std:c++17 /EHsc /MT /DNOMINMAX /DUNICODE /I WiNILL\System tests\pane_content_preview.cpp /Fo`"$out\\`" /Fe`"$out\pane_content_preview.exe`" user32.lib gdi32.lib shell32.lib"
    & cmd.exe /d /s /c $command
    if ($LASTEXITCODE -ne 0) { throw 'pane content preview build failed' }
} finally { Pop-Location }
& "$out\pane_content_preview.exe" $OutDir
if ($LASTEXITCODE -ne 0) { throw 'pane content preview failed' }
