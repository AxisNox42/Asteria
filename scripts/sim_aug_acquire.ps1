# Offline augment-acquisition simulation: N runs per difficulty through the real
# card draw and acquire rules (tests/aug_acquire_sim.cpp). Writes
# build\sim\aug-acquire\aug_acquire_<DIFFICULTY>.csv and prints a summary.
param([int]$Runs = 1000)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vs = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $vs) { throw 'Visual Studio C++ tools required' }
$vcvars = Join-Path $vs 'VC\Auxiliary\Build\vcvars64.bat'
$out = Join-Path $root 'build\sim\aug-acquire'
New-Item -ItemType Directory -Force $out | Out-Null
Push-Location $root
try {
    $includes = '/I Include /I WiNILL\Core /I WiNILL\Core\Scenes /I WiNILL\Render /I WiNILL\Entity /I WiNILL\System'
    $sources = 'tests\aug_acquire_sim.cpp WiNILL\Core\GameManager.cpp WiNILL\Entity\AugmentCatalog.cpp WiNILL\Entity\PassiveAugments.cpp WiNILL\Entity\ActiveSkills.cpp WiNILL\Entity\SynergyRuntime.cpp glad.c'
    $command = "call `"$vcvars`" >nul && cl /nologo /utf-8 /std:c++17 /EHsc /MT /DNOMINMAX /O2 $includes $sources /Fo`"$out/`" /Fe`"$out\aug_acquire_sim.exe`" /link Lib\glfw3_mt.lib opengl32.lib user32.lib gdi32.lib shell32.lib winmm.lib"
    & cmd.exe /d /s /c $command
    if ($LASTEXITCODE -ne 0) { throw 'aug acquire sim build failed' }
    Push-Location $out
    try { & .\aug_acquire_sim.exe $Runs } finally { Pop-Location }
} finally { Pop-Location }
