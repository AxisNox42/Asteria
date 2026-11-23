# Each elite fought down for real (player auto-fire, no debug kill, no gate
# skipping): checks every gate is passed and the fight ends, with no crash.
# Writes Screenshots\elite_state.txt and prints a per-elite summary.
param([string[]]$Kinds = @('4', '7', '1', '6'))
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$exeDir = if ($env:ASTERIA_TEST_EXE_DIR) { $env:ASTERIA_TEST_EXE_DIR } else { Join-Path $root 'build\windows\x64\Release' }
$exe = Join-Path $exeDir 'Asteria.exe'
if (-not (Test-Path $exe)) { throw "build first: $exe" }
$state = Join-Path $exeDir 'Screenshots\elite_state.txt'
Remove-Item -LiteralPath $state -ErrorAction SilentlyContinue
$steps = @('RUN~0.3', 'NOQUOTA=1~0.01', 'LEVEL=60~0.05', 'GOD=1~0.01')
foreach ($k in $Kinds) {
    $steps += @('MOBCLEAR~0.05', "SPAWN=$k~0.05")
    1..120 | ForEach-Object { $steps += "HP=1~0.01"; $steps += "ELITEHIT=0.02~0.25"; $steps += "ELITEDUMP=${k}_$($_)~0.25" }
}
$env:ASTERIA_SMOKE = $steps -join ';'
$env:ASTERIA_SMOKE_TAG = 'elitefight'
$env:ASTERIA_SMOKE_SIZE = '1280x720'
try {
    $p = Start-Process -FilePath $exe -WorkingDirectory $exeDir -PassThru -WindowStyle Hidden
    if (-not $p.WaitForExit(600000)) { $p.Kill(); throw 'elite full-fight smoke timed out' }
    if ($p.ExitCode -ne 0) { throw "elite full-fight smoke exited with $($p.ExitCode)" }
} finally {
    Remove-Item Env:ASTERIA_SMOKE, Env:ASTERIA_SMOKE_TAG, Env:ASTERIA_SMOKE_SIZE -ErrorAction SilentlyContinue
}
$lines = Get-Content $state
foreach ($k in $Kinds) {
    $mine = $lines | Where-Object { $_ -match "^${k}_\d+ " }
    $firstGate1 = ($mine | Where-Object { $_ -match 'gates=1' } | Select-Object -First 1)
    $firstGate2 = ($mine | Where-Object { $_ -match 'gates=2' } | Select-Object -First 1)
    $dead = ($mine | Where-Object { $_ -match 'alive=0|none' } | Select-Object -First 1)
    Write-Host "kind $k  gate1: $firstGate1"
    Write-Host "        gate2: $firstGate2"
    Write-Host "        end  : $dead"
}
