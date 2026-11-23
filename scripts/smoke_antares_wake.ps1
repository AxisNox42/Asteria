# Nightmare ANTARES wake, look pass (Docs/Plans/2026-11-23_안타레스_난이도별_세부.md):
# spawned alone (SPAWN=7) on NIGHTMARE at phase 1, 2, 3, then killed at phase 3 so its fragments clash; captured every 0.25 s while the
# player moves. Writes Screenshots\antareswake_phase<n>.png. Silent,
# like every smoke run; the game moves itself to virtual desktop 2.
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$exeDir = if ($env:ASTERIA_TEST_EXE_DIR) { $env:ASTERIA_TEST_EXE_DIR } else { Join-Path $root 'build\windows\x64\Release' }
$exe = Join-Path $exeDir 'Asteria.exe'
if (-not (Test-Path $exe)) { throw "build first: $exe" }
$shots = Join-Path $exeDir 'Screenshots'
Remove-Item (Join-Path $shots 'antareswake_*') -ErrorAction SilentlyContinue
$frames = 48
$steps = @('DIFF=NIGHTMARE~0.01', 'RUN~0.3', 'DIFF=NIGHTMARE~0.01', 'NOQUOTA=1~0.01', 'LEVEL=30~0.05', 'GOD=1~0.01')
foreach ($gate in 0, 1, 2, 3) {
    $steps += @('MOBCLEAR~0.05', 'SPAWN=7~0.05', 'MOBHP=1000000~0.02')
    if ($gate -gt 0) { $steps += "MOBGATE=$([Math]::Min($gate, 2))~0.02" }
    $steps += 'MOBPLACE=700~0.02'
    if ($gate -eq 3) { $steps += @('MOBHP=1~0.02', 'FIRE=1~0.01') }   # dies to the first shot: fragments
    1..$frames | ForEach-Object {
        $steps += 'HP=1~0.01'
        # Fragments: stop shooting once they are out, so they live and clash.
        if ($gate -eq 3 -and $_ -eq 4) { $steps += @('FIRE=0~0.01', 'MOBHP=1000000~0.01') }
        # Antares put back 700 px off every 4 s so runs get fast; the player sidesteps up and down.
        if ($_ % 16 -eq 1 -and $gate -lt 3) { $steps += 'MOBPLACE=700~0.01' }
        if ($_ % 8 -eq 1) { $steps += 'WALK=U~0.01' } elseif ($_ % 8 -eq 5) { $steps += 'WALK=D~0.01' }
        $steps += "S_${gate}_$($_)~0.25"
    }
    $steps += @('WALK=0~0.01', 'FIRE=0~0.01')
}
$env:ASTERIA_SMOKE = $steps -join ';'
$env:ASTERIA_SMOKE_TAG = 'antareswake'
$env:ASTERIA_SMOKE_SIZE = '1280x720'
$saveDir = Join-Path $env:TEMP 'asteria_smoke_antareswake_save'
New-Item -ItemType Directory -Force $saveDir | Out-Null
$env:ASTERIA_SAVE_DIR = $saveDir
try {
    $p = Start-Process -FilePath $exe -WorkingDirectory $exeDir -PassThru -WindowStyle Hidden
    if (-not $p.WaitForExit(360000)) { $p.Kill(); throw 'antares wake smoke timed out' }
    if ($p.ExitCode -ne 0) { throw "antares wake smoke exited with $($p.ExitCode)" }
} finally {
    Remove-Item Env:ASTERIA_SMOKE, Env:ASTERIA_SMOKE_TAG, Env:ASTERIA_SMOKE_SIZE, Env:ASTERIA_SAVE_DIR -ErrorAction SilentlyContinue
}
$py = @'
import glob, sys
from PIL import Image
d, frames = sys.argv[1], int(sys.argv[2])
for gate in (0, 1, 2, 3):
    picks = list(range(3, frames + 1, 3))
    cols = 4
    W, H = 480, 270
    sheet = Image.new('RGB', (W * cols, H * ((len(picks) + cols - 1) // cols)))
    for n, i in enumerate(picks):
        f = glob.glob(f'{d}/antareswake_*_S_{gate}_{i}.png')
        if f: sheet.paste(Image.open(f[0]).convert('RGB').resize((W, H)), ((n % cols) * W, (n // cols) * H))
    sheet.save(f'{d}/antareswake_phase{gate + 1}.png')
'@
$pyFile = Join-Path $env:TEMP 'antareswake_sheet.py'
Set-Content -LiteralPath $pyFile -Value $py -Encoding utf8
& python $pyFile $shots $frames
Get-ChildItem (Join-Path $shots 'antareswake_phase*.png') | ForEach-Object { Write-Host $_.FullName }
