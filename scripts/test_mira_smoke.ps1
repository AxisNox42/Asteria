# Live-game smoke for the Mira corruption walker: the debug-menu spawn path brings it in
# from the screen edge, then it comes into view fires its twin guns and
# launches craft. Writes Screenshots\mira_*.png and Screenshots\mob_state.txt.
# Needs build\windows\x64\Release\WiNILL.exe. Uses a throwaway save folder.
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$exeDir = if ($env:MIRA_EXE_DIR) { $env:MIRA_EXE_DIR } else { Join-Path $root 'buildwindowsdRelease' }
$exe = Join-Path $exeDir 'WiNILL.exe'
if (-not (Test-Path $exe)) { throw "build first: $exe" }
$shots = Join-Path $exeDir 'Screenshots'
Remove-Item (Join-Path $shots 'mira_*') -ErrorAction SilentlyContinue
Remove-Item (Join-Path $shots 'mob_state.txt') -ErrorAction SilentlyContinue
$saveDir = Join-Path $env:TEMP "asteria-mira-smoke-$PID"
New-Item -ItemType Directory -Force $saveDir | Out-Null
$steps = @('RUN~0.3', 'LEVEL=80~0.05', 'MOBCLEAR~0.05', 'DEBUGSPAWN=12~0.05')
function Hold($seconds) {
    foreach ($i in 1..[int]($seconds * 2)) { $script:steps += 'HP=1~0.02'; $script:steps += 'MOBHP=1000000~0.48' }
}
$t = 0
foreach ($mark in 4, 8, 12, 20) {
    Hold ($mark - $t)
    $t = $mark
    $steps += "MOBDUMP=T$mark~0.02"
    $steps += "S_$mark~0.05"
}
# kill it: the zone must stay where it fell, then thin out and vanish
$steps += 'MOBKILL~0.3'
$steps += 'S_dead0~0.05'
$steps += 'HP=1~1.4'
$steps += 'S_dead1~0.05'
$steps += 'HP=1~2.2'
$steps += 'S_dead2~0.05'
$env:ASTERIA_SMOKE = $steps -join ';'
$env:ASTERIA_SMOKE_TAG = 'mira'
$env:ASTERIA_SMOKE_SIZE = '1280x720'
$env:ASTERIA_SAVE_DIR = $saveDir
try {
    $p = Start-Process -FilePath $exe -WorkingDirectory $exeDir -PassThru -WindowStyle Hidden
    if (-not $p.WaitForExit(120000)) { $p.Kill(); throw 'mira smoke timed out' }
    if ($p.ExitCode -ne 0) { throw "mira smoke exited with $($p.ExitCode)" }
} finally {
    Remove-Item Env:ASTERIA_SMOKE, Env:ASTERIA_SMOKE_TAG, Env:ASTERIA_SMOKE_SIZE, Env:ASTERIA_SAVE_DIR -ErrorAction SilentlyContinue
    Remove-Item -Recurse -Force $saveDir -ErrorAction SilentlyContinue
}
Get-Content (Join-Path $shots 'mob_state.txt') | Select-String 'mira' | Select-Object -First 30
Get-ChildItem (Join-Path $shots 'mira_*') | Select-Object Name, Length
