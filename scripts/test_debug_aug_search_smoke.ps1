# Smoke for the F1 debug console > 증강 tab search box: empty, an English query,
# a Korean query and a query with no match. Writes Screenshots\dbgaug_*.png.
# Uses a throwaway save folder; runs on virtual desktop 2 (smoke mode), silent.
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$exeDir = if ($env:ASTERIA_TEST_EXE_DIR) { $env:ASTERIA_TEST_EXE_DIR } else { Join-Path $root 'build\windows\x64\Release' }
$exe = Join-Path $exeDir 'Asteria.exe'
if (-not (Test-Path $exe)) { throw "build first: $exe" }
$shots = Join-Path $exeDir 'Screenshots'
Remove-Item (Join-Path $shots 'dbgaug_*') -ErrorAction SilentlyContinue
$saveDir = Join-Path $env:TEMP "asteria-dbgaug-smoke-$PID"
New-Item -ItemType Directory -Force $saveDir | Out-Null
$steps = @(
    'RUN~0.3',
    'DEBUGAUG=~0.3', 'S_dbgaug_empty~0.05',
    'DEBUGAUG=PIERCE~0.3', 'S_dbgaug_en~0.05',
    'DEBUGAUG=관통~0.3', 'S_dbgaug_kr~0.05',
    'DEBUGAUG=zzzz~0.3', 'S_dbgaug_none~0.05',
    # OFF: a frame (left click toggles), then a stat card (left adds, right takes one back)
    'DEBUGAUG=레이저~0.2', 'DEBUGAUGCLICK=0~0.3', 'S_dbgaug_on~0.05',
    'DEBUGAUGCLICK=0~0.3', 'S_dbgaug_off~0.05',
    'DEBUGAUG=~0.1', 'DEBUGAUGCLICK=0~0.1', 'DEBUGAUGCLICK=0~0.3', 'S_dbgaug_stat2~0.05',
    'DEBUGAUGCLICK=0R~0.3', 'S_dbgaug_stat1~0.05'
)
$env:ASTERIA_SMOKE = $steps -join ';'
$env:ASTERIA_SMOKE_TAG = 'dbgaug'
$env:ASTERIA_SMOKE_SIZE = '1600x900'
$env:ASTERIA_SAVE_DIR = $saveDir
try {
    $p = Start-Process -FilePath $exe -WorkingDirectory $exeDir -PassThru -WindowStyle Hidden
    if (-not $p.WaitForExit(90000)) { $p.Kill(); throw 'debug aug search smoke timed out' }
    if ($p.ExitCode -ne 0) { throw "debug aug search smoke exited with $($p.ExitCode)" }
} finally {
    Remove-Item Env:ASTERIA_SMOKE, Env:ASTERIA_SMOKE_TAG, Env:ASTERIA_SMOKE_SIZE, Env:ASTERIA_SAVE_DIR -ErrorAction SilentlyContinue
    Remove-Item -Recurse -Force $saveDir -ErrorAction SilentlyContinue
}
Get-ChildItem (Join-Path $shots 'dbgaug_*') | Select-Object Name, Length
