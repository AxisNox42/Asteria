# Shotgun feel: nearest-first hits along a step, pierce along a step, the shotgun pellets (tests/shot_order_test.cpp).
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$installer = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer"
$env:PATH = "$installer;$env:PATH"
$vs = & (Join-Path $installer 'vswhere.exe') -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $vs) { throw 'Visual Studio C++ tools required' }
$vcvars = Join-Path $vs 'VC\Auxiliary\Build\vcvars64.bat'
$out = Join-Path $root 'build\tests\shot_order'
New-Item -ItemType Directory -Force $out | Out-Null
Push-Location $root
try {
    $includes = '/I Include /I WiNILL\Core /I WiNILL\Core\Scenes /I WiNILL\Render /I WiNILL\Entity /I WiNILL\System'
    $sources = 'tests\shot_order_test.cpp WiNILL\Core\GameManager.cpp WiNILL\Entity\AugmentCatalog.cpp WiNILL\Entity\PassiveAugments.cpp WiNILL\Entity\ActiveSkills.cpp WiNILL\Entity\SynergyRuntime.cpp glad.c'
    $command = "call `"$vcvars`" >nul && cl /nologo /utf-8 /std:c++17 /EHsc /MT /DNOMINMAX $includes $sources /Fo`"$out/`" /Fe`"$out\shot_order_test.exe`" /link Lib\glfw3_mt.lib opengl32.lib user32.lib gdi32.lib shell32.lib winmm.lib"
    & cmd.exe /d /s /c $command
    if ($LASTEXITCODE -ne 0) { throw 'shot order test build failed' }
} finally { Pop-Location }

# Nothing here saves, but keep any save write away from the real one.
$sandbox = Join-Path $out 'sandbox'
New-Item -ItemType Directory -Force $sandbox | Out-Null
$env:ASTERIA_SAVE_DIR = $sandbox
Push-Location $sandbox
try {
    & "$out\shot_order_test.exe"
    if ($LASTEXITCODE -ne 0) { throw 'shot order test failed' }
} finally { Pop-Location; Remove-Item Env:ASTERIA_SAVE_DIR -ErrorAction SilentlyContinue }
