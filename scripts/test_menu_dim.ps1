$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$installer = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer"
$vswhere = Join-Path $installer 'vswhere.exe'
# vcvars64.bat resolves vswhere through PATH.
$env:PATH = "$installer;$env:PATH"
$vs = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $vs) { throw 'Visual Studio C++ tools required' }
$vcvars = Join-Path $vs 'VC\Auxiliary\Build\vcvars64.bat'
$out = Join-Path $root 'build\tests\menu-dim'
New-Item -ItemType Directory -Force $out | Out-Null
Push-Location $root
try {
    $includes = '/I Include /I WiNILL\Core /I WiNILL\Core\Scenes /I WiNILL\Render /I WiNILL\Entity /I WiNILL\System'
    $command = "call `"$vcvars`" >nul && cl /nologo /utf-8 /std:c++17 /EHsc /MT /DNOMINMAX $includes tests\menu_dim_test.cpp WiNILL\Entity\AugmentCatalog.cpp /Fo`"$out\\`" /Fe`"$out\menu_dim_test.exe`""
    & cmd.exe /d /s /c $command
    if ($LASTEXITCODE -ne 0) { throw 'menu dim test build failed' }
} finally { Pop-Location }

# The save path is relative: run in an empty folder, far from the real save.
$sandbox = Join-Path $out 'sandbox'
if (Test-Path $sandbox) { Remove-Item -LiteralPath $sandbox -Recurse -Force }
New-Item -ItemType Directory -Force $sandbox | Out-Null
Push-Location $sandbox
try {
    & "$out\menu_dim_test.exe"
    if ($LASTEXITCODE -ne 0) { throw 'menu dim test failed' }
} finally { Pop-Location }
