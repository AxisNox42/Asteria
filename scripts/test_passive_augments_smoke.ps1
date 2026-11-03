# Passive augments smoke: every passive card at once (GIVE=ALL), then a run
# that fires at the nearest enemy and takes the first card of every pick
# (FIRE=1). Checks that the run does not crash, the stat cards applied
# (max HP 100 + Max HP Up 15 + Graze Recovery 10 = 125) and enemies die.
# Saves are untouched: smoke sessions skip SaveGame.
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$exeDir = Join-Path $root 'build\windows\x64\Release'
$exe = Join-Path $exeDir 'WiNILL.exe'
if (-not (Test-Path $exe)) { throw "build first: $exe" }
$stateFile = Join-Path $exeDir 'Screenshots\reg04_state.txt'
Remove-Item -LiteralPath $stateFile -ErrorAction SilentlyContinue

$steps = 'RUN~0.3;REG04DUMP=BEFORE~0.05;GIVE=ALL~0.05;REG04DUMP=GIVEN~0.05;FIRE=1~0.05'
for ($i = 1; $i -le 10; $i++) { $steps += ";REG04DUMP=T$i~2.0" }
$env:ASTERIA_SMOKE = $steps
$env:ASTERIA_SMOKE_TAG = 'passive'
$env:ASTERIA_SMOKE_SIZE = '1280x720'
try {
    $p = Start-Process -FilePath $exe -WorkingDirectory $exeDir -PassThru -WindowStyle Hidden
    if (-not $p.WaitForExit(180000)) { $p.Kill(); throw 'passive smoke timed out' }
    if ($p.ExitCode -ne 0) { throw "passive smoke exited with $($p.ExitCode)" }
} finally {
    Remove-Item Env:ASTERIA_SMOKE, Env:ASTERIA_SMOKE_TAG, Env:ASTERIA_SMOKE_SIZE -ErrorAction SilentlyContinue
    Get-ChildItem (Join-Path $exeDir 'Screenshots') -Filter 'passive_*.png' -ErrorAction SilentlyContinue | Remove-Item
}
if (-not (Test-Path $stateFile)) { throw 'no passive smoke snapshot written' }

$snap = @{}
foreach ($line in Get-Content $stateFile) {
    $parts = $line -split ' '
    $kv = @{}
    foreach ($p in $parts[1..($parts.Count - 1)]) { $k, $v = $p -split '=', 2; $kv[$k] = $v }
    $snap[$parts[0]] = $kv
}
Get-Content $stateFile | ForEach-Object { Write-Host $_ }
if ($snap.BEFORE.hp -ne '100.0') { throw "start HP $($snap.BEFORE.hp), expected 100.0" }
if ($snap.GIVEN.hp -ne '125.0') { throw "HP after GIVE=ALL $($snap.GIVEN.hp), expected 125.0" }
if ([int]$snap.T10.kills -le 0) { throw 'no kills with every passive on' }
Write-Host "Passive augments smoke passed ($($snap.T10.kills) kills, wave $($snap.T10.wave))"
