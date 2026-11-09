# Live-game smoke for TRIANGULUM: it walks at the player with no attack; when it
# dies it freezes and shakes, then three Swarms scatter out of the spot. Writes Screenshots\triangulum_*.png and
# Screenshots\mob_state.txt. Needs a WiNILL.exe in TRIANGULUM_EXE_DIR (default
# build\windows\x64\Release). Uses a throwaway save folder; the run is silent.
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$exeDir = if ($env:TRIANGULUM_EXE_DIR) { $env:TRIANGULUM_EXE_DIR } else { Join-Path $root 'build\windows\x64\Release' }
$exe = Join-Path $exeDir 'WiNILL.exe'
if (-not (Test-Path $exe)) { throw "build first: $exe" }
$shots = Join-Path $exeDir 'Screenshots'
Remove-Item (Join-Path $shots 'triangulum_*') -ErrorAction SilentlyContinue
Remove-Item (Join-Path $shots 'mob_state.txt') -ErrorAction SilentlyContinue
$saveDir = Join-Path $env:TEMP "asteria-triangulum-smoke-$PID"
New-Item -ItemType Directory -Force $saveDir | Out-Null
$steps = @('RUN~0.3', 'LEVEL=80~0.05', 'MOBCLEAR~0.05', 'SPAWN=13~0.05')
function Hold($seconds) {
    foreach ($i in 1..[int]($seconds * 2)) { $script:steps += 'HP=1~0.02'; $script:steps += 'MOBHP=1000000~0.48' }
}
Hold 2
$steps += 'MOBDUMP=alive~0.02'
$steps += 'S_alive~0.12'
# a few more frames to judge the motion (shoving corners, rattling Swarms)
$steps += 'S_alive2~0.12'
$steps += 'S_alive3~0.12'
$steps += 'S_alive4~0.05'
# kill it: the body freezes and shakes (~0.45 s), then the shell breaks and three
# Swarms scatter within 50 px before they chase
# (each capture costs game time too: read the times in mob_state.txt)
$steps += 'MOBKILL~0.02'
$steps += 'MOBDUMP=kill~0.01'
$steps += 'S_hold1~0.03'
$steps += 'MOBDUMP=hold1~0.01'
$steps += 'S_hold2~0.12'
$steps += 'MOBDUMP=hold2~0.01'
$steps += 'S_burst~0.15'
$steps += 'MOBDUMP=burst~0.01'
$steps += 'S_dead1~0.35'
$steps += 'MOBDUMP=dead1~0.01'
$env:ASTERIA_SMOKE = $steps -join ';'
$env:ASTERIA_SMOKE_TAG = 'triangulum'
$env:ASTERIA_SMOKE_SIZE = '1280x720'
$env:ASTERIA_SAVE_DIR = $saveDir
try {
    $p = Start-Process -FilePath $exe -WorkingDirectory $exeDir -PassThru -WindowStyle Hidden
    if (-not $p.WaitForExit(120000)) { $p.Kill(); throw 'triangulum smoke timed out' }
    if ($p.ExitCode -ne 0) { throw "triangulum smoke exited with $($p.ExitCode)" }
} finally {
    Remove-Item Env:ASTERIA_SMOKE, Env:ASTERIA_SMOKE_TAG, Env:ASTERIA_SMOKE_SIZE, Env:ASTERIA_SAVE_DIR -ErrorAction SilentlyContinue
    Remove-Item -Recurse -Force $saveDir -ErrorAction SilentlyContinue
}
Get-Content (Join-Path $shots 'mob_state.txt') | Select-Object -First 40
Get-ChildItem (Join-Path $shots 'triangulum_*') | Select-Object Name, Length
