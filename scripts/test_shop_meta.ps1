$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$installer = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer"
$vswhere = Join-Path $installer 'vswhere.exe'
# vcvars64.bat resolves vswhere through PATH.
$env:PATH = "$installer;$env:PATH"
$vs = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $vs) { throw 'Visual Studio C++ tools required' }
$vcvars = Join-Path $vs 'VC\Auxiliary\Build\vcvars64.bat'
$out = Join-Path $root 'build\tests\shop-meta'
New-Item -ItemType Directory -Force $out | Out-Null
Push-Location $root
try {
    $includes = '/I Include /I WiNILL\Core /I WiNILL\Core\Scenes /I WiNILL\Render /I WiNILL\Entity /I WiNILL\System'
    $command = "call `"$vcvars`" >nul && cl /nologo /utf-8 /std:c++17 /EHsc /MT /DNOMINMAX $includes tests\shop_meta_test.cpp /Fo`"$out\\`" /Fe`"$out\shop_meta_test.exe`""
    & cmd.exe /d /s /c $command
    if ($LASTEXITCODE -ne 0) { throw 'shop meta test build failed' }
} finally { Pop-Location }

# The save round trip writes only into this empty sandbox, never the real save.
$sandbox = Join-Path $out 'sandbox'
if (Test-Path $sandbox) { Remove-Item -LiteralPath $sandbox -Recurse -Force }
New-Item -ItemType Directory -Force $sandbox | Out-Null
$prev = $env:ASTERIA_SAVE_DIR
$env:ASTERIA_SAVE_DIR = $sandbox
Push-Location $sandbox
try {
    & "$out\shop_meta_test.exe"
    if ($LASTEXITCODE -ne 0) { throw 'shop meta test failed' }
} finally {
    Pop-Location
    if ($null -eq $prev) { Remove-Item Env:ASTERIA_SAVE_DIR -ErrorAction SilentlyContinue } else { $env:ASTERIA_SAVE_DIR = $prev }
}

# Upgrades whose call site is in a .cpp the test cannot link: pin that the
# game still calls the helper the test checked.
Push-Location $root
try {
    $pins = @(
        @('WiNILL\Entity\ActiveSkills.cpp',    'MetaCooldownMult\(\)',              1, 'Skill Cooldown'),
        @('WiNILL\Entity\ActiveSkills.cpp',    'MetaDurationMult\(\)',              6, 'Duration (6 lasting skill objects)'),
        @('WiNILL\Entity\PassiveAugments.cpp', 'MetaLuck\(\)',                      1, 'Luck'),
        @('WiNILL\Entity\PassiveAugments.cpp', 'g_PlayerWaveWard',                  1, 'Wave Ward (blocks a hit)'),
        @('WiNILL\main.cpp',                   'g_PlayerWaveWard = MetaWaveWard\(\)', 2, 'Wave Ward (run start + every wave)'),
        @('WiNILL\main.cpp',                   'MetaBossPrepFrac\(\)',              1, 'Boss Prep'),
        @('WiNILL\main.cpp',                   'MetaWaveMendFrac\(\)',              1, 'Wave Mend'),
        @('WiNILL\main.cpp',                   'g_MetaPlayerHpFrac = ',             2, 'Last Stand HP feed'),
        @('WiNILL\main.cpp',                   'g_MetaComboNow = g_Combo',          1, 'Combo Focus combo feed'),
        @('WiNILL\main.cpp',                   'g_ComboWindowBonus = MetaComboWindowBonus\(\)', 1, 'Combo Focus window'),
        @('WiNILL\main.cpp',                   'g_MetaFirstCardPending = ',         1, 'First Card Boon (armed)'),
        @('WiNILL\Core\GameManager.cpp',       'g_MetaFirstCardPending',            1, 'First Card Boon (spent)')
    )
    foreach ($p in $pins) {
        $n = @(Select-String -Path $p[0] -Pattern $p[1]).Count
        if ($n -lt $p[2]) { throw "$($p[3]): expected $($p[2])+ uses of '$($p[1])' in $($p[0]), found $n" }
        Write-Host ("source pin ok: {0} ({1} in {2})" -f $p[3], $n, (Split-Path $p[0] -Leaf))
    }
} finally { Pop-Location }
