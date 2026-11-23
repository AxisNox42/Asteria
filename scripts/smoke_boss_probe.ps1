# Boss movement-demand probe (Docs/Plans/2026-11-17_보스전_15분_집중도.md).
# One smoke session: a boss with one pattern forced (or its natural flow), the
# player held still or walked left/right, boss and player sampled about every
# 0.4 s. Shows which patterns can be stood through. Read with smoke_boss_probe.py.
# Usage: smoke_boss_probe.ps1 -Boss NORMA|CAELUM -Arg <pattern|PANES|NONE> -Policy still|walk -Seconds 8
#        [-Pre 'NORMATEST=HP60~0.5'] [-OutDir <dir>]
# Runs on virtual desktop 2, silent. Honours ASTERIA_TEST_EXE_DIR.
param(
    [ValidateSet('NORMA','CAELUM')][string]$Boss = 'NORMA',
    [string]$Arg = 'PANES',            # pattern name, or PANES / NONE for the natural flow
    [ValidateSet('still','walk','walkfast')][string]$Policy = 'still',
    [int]$Seconds = 10,
    [string]$Pre = '',                 # extra steps before the pattern (e.g. NORMATEST=P2~1)
    [string]$OutDir = (Join-Path (Split-Path $PSScriptRoot -Parent) 'build\smoke\boss_probe')
)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$exeDir = if ($env:ASTERIA_TEST_EXE_DIR) { $env:ASTERIA_TEST_EXE_DIR } else { Join-Path $root 'build\windows\x64\Release' }
$exe = Join-Path $exeDir 'Asteria.exe'
$shots = Join-Path $exeDir 'Screenshots'
$tag = "bossprobe"
New-Item -ItemType Directory -Force $OutDir | Out-Null
Remove-Item -LiteralPath (Join-Path $shots 'norma_state.txt'), (Join-Path $shots 'mob_state.txt') -ErrorAction SilentlyContinue
$tele = Join-Path $shots 'smoke_telemetry.txt'
Remove-Item -LiteralPath $tele -ErrorAction SilentlyContinue

$steps = @('RUN~0.3')
if ($Boss -eq 'NORMA') { $steps += 'NORMATEST=PANES~1.5' } else { $steps += 'CAELUMTEST=NONE~1.5' }
if ($Pre) { $steps += $Pre }
$steps += 'FIRE=1~0.02'
if ($Boss -eq 'NORMA' -and $Arg -ne 'PANES') { $steps += "NORMATEST=$Arg~0.02" }
if ($Boss -eq 'CAELUM' -and $Arg -ne 'NONE') { $steps += "CAELUMTEST=$Arg~0.02" }
$period = if ($Policy -eq 'walkfast') { 3 } else { 8 }   # samples per direction (0.25 s each)
$dirs = 'L','R'
$n = $Seconds * 4
for ($i = 0; $i -lt $n; $i++) {
    if ($Policy -ne 'still' -and ($i % $period) -eq 0) {
        $steps += "WALK=$($dirs[[int][math]::Floor($i / $period) % 2])~0.01"
    }
    if ($Boss -eq 'NORMA') { $steps += "NORMADUMP=T$i~0.01" }
    $steps += "MOBDUMP=T$i~0.2"
}
$env:ASTERIA_SMOKE = $steps -join ';'
$env:ASTERIA_SMOKE_TAG = $tag
$env:ASTERIA_SMOKE_SIZE = '1280x720'
$env:ASTERIA_SMOKE_FIGHTLOG = '1'
try {
    $p = Start-Process -FilePath $exe -WorkingDirectory $exeDir -PassThru -WindowStyle Hidden
    if (-not $p.WaitForExit(($Seconds + 60) * 3000)) { $p.Kill(); throw 'probe timed out' }
    $code = $p.ExitCode
} finally {
    Remove-Item Env:ASTERIA_SMOKE, Env:ASTERIA_SMOKE_TAG, Env:ASTERIA_SMOKE_SIZE, Env:ASTERIA_SMOKE_FIGHTLOG -ErrorAction SilentlyContinue
}
$name = "$Boss`_$Arg`_$Policy"
foreach ($f in 'norma_state.txt','mob_state.txt') {
    $src = Join-Path $shots $f
    if (Test-Path $src) { Copy-Item $src (Join-Path $OutDir "$name`_$f") -Force }
}
# the run's own play log (FIGHT_* lines)
if (Test-Path $tele) { Copy-Item $tele (Join-Path $OutDir "$name`_telemetry.txt") -Force }
# keep 1 screenshot in 20, drop the rest of this run's captures
$pngs = Get-ChildItem $shots -Filter "$tag`_*.png" | Sort-Object Name
$keepDir = Join-Path $OutDir "$name`_png"
New-Item -ItemType Directory -Force $keepDir | Out-Null
$k = 0
foreach ($png in $pngs) {
    if ($png.Name -match 'MOBDUMP' -and ($k++ % 20) -eq 0) { Copy-Item $png.FullName $keepDir -Force }
    Remove-Item $png.FullName -Force
}
"$name exit=$code steps=$($steps.Count)"
