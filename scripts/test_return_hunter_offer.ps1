# Boomerang / Relentless Hunter Rounds are never offered together
# (tests/return_hunter_offer_test.cpp, through the real SynergyRuntime CanOffer).
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vs = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $vs) { throw 'Visual Studio C++ tools required' }
$vcvars = Join-Path $vs 'VC\Auxiliary\Build\vcvars64.bat'
$out = Join-Path $root 'build\tests\return-hunter-offer'
New-Item -ItemType Directory -Force $out | Out-Null
Push-Location $root
try {
    $includes = '/I Include /I WiNILL\Core /I WiNILL\Core\Scenes /I WiNILL\Render /I WiNILL\Entity /I WiNILL\System'
    $sources = 'tests\return_hunter_offer_test.cpp WiNILL\Core\GameManager.cpp WiNILL\Entity\AugmentCatalog.cpp WiNILL\Entity\PassiveAugments.cpp WiNILL\Entity\ActiveSkills.cpp WiNILL\Entity\SynergyRuntime.cpp glad.c'
    $command = "call `"$vcvars`" >nul && cl /nologo /utf-8 /std:c++17 /EHsc /MT /DNOMINMAX $includes $sources /Fo`"$out/`" /Fe`"$out\return_hunter_offer_test.exe`" /link Lib\glfw3_mt.lib opengl32.lib user32.lib gdi32.lib shell32.lib winmm.lib"
    & cmd.exe /d /s /c $command
    if ($LASTEXITCODE -ne 0) { throw 'return/hunter offer test build failed' }
    Push-Location $out
    try {
        & .\return_hunter_offer_test.exe
        if ($LASTEXITCODE -ne 0) { throw 'return/hunter offer test failed' }
    } finally { Pop-Location }
} finally { Pop-Location }
