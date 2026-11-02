# Active skills smoke: equips every active skill, three at a time (GIVE=),
# and lets the run use them as soon as they are ready while firing at the
# nearest enemy (FIRE=1 also takes the first card of every pick). Run once
# with actives only and once with every passive card as well (GIVE=ALL).
# Checks that the game does not crash and that enemies die.
# Saves are untouched: smoke sessions skip SaveGame.
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$exeDir = Join-Path $root 'build\windows\x64\Release'
$exe = Join-Path $exeDir 'WiNILL.exe'
if (-not (Test-Path $exe)) { throw "build first: $exe" }
$stateFile = Join-Path $exeDir 'Screenshots\reg04_state.txt'

# Active codes in AugType order (Augment.h).
$header = Get-Content (Join-Path $root 'WiNILL\Entity\Augment.h') -Raw
$actives = [regex]::Matches($header, '\bA_[A-Z_]+\b') | ForEach-Object { $_.Value } | Select-Object -Unique
if ($actives.Count -ne 35) { throw "expected 35 active codes, found $($actives.Count)" }

# order: the active codes in the order to equip them. GOD=1 keeps the player
# alive so every group gets used.
function Run-Smoke([string]$label, [string]$prefix, [string[]]$order) {
    Remove-Item -LiteralPath $stateFile -ErrorAction SilentlyContinue
    $steps = "RUN~0.3;$prefix" + 'GOD=1~0.05;FIRE=1~0.05'
    for ($i = 0; $i -lt $order.Count; $i += 3) {
        $group = $order[$i..([Math]::Min($i + 2, $order.Count - 1))]
        foreach ($code in $group) { $steps += ";GIVE=$code~0.05" }
        $steps += ";REG04DUMP=G$i~4.0"
    }
    $env:ASTERIA_SMOKE = $steps
    $env:ASTERIA_SMOKE_TAG = 'active'
    $env:ASTERIA_SMOKE_SIZE = '1280x720'
    try {
        $p = Start-Process -FilePath $exe -WorkingDirectory $exeDir -PassThru -WindowStyle Hidden
        if (-not $p.WaitForExit(300000)) { $p.Kill(); throw "$label smoke timed out" }
        if ($p.ExitCode -ne 0) { throw "$label smoke exited with $($p.ExitCode)" }
    } finally {
        Remove-Item Env:ASTERIA_SMOKE, Env:ASTERIA_SMOKE_TAG, Env:ASTERIA_SMOKE_SIZE -ErrorAction SilentlyContinue
        Get-ChildItem (Join-Path $exeDir 'Screenshots') -Filter 'active_*.png' -ErrorAction SilentlyContinue | Remove-Item
    }
    if (-not (Test-Path $stateFile)) { throw "$label smoke wrote no snapshot" }
    $lines = Get-Content $stateFile
    $lines | ForEach-Object { Write-Host "$label $_" }
    $last = $lines[-1]
    $kills = [int]([regex]::Match($last, ' kills=(\d+)').Groups[1].Value)
    if ($kills -le 0) { throw "$label smoke: no kills" }
    Write-Host "$label smoke passed ($kills kills)"
}

Run-Smoke 'actives' '' $actives
# Reversed with every passive on: the run is often won early, so the last
# actives get their turn here.
$reversed = [string[]]$actives.Clone()
[Array]::Reverse($reversed)
Run-Smoke 'all' 'GIVE=ALL~0.05;' $reversed
Write-Host 'Active skills smoke passed'
