# HOROLOGIUM clock elite, look pass (Docs/Plans/2026-11-18_새엘리트_재시작.md):
# spawned alone (SPAWN=14) at phase 1, 2, 3, captured every 0.25 s while the
# player moves. Writes Screenshots\horologium_phase<n>.png. Silent,
# like every smoke run; the game moves itself to virtual desktop 2.
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$exeDir = if ($env:ASTERIA_TEST_EXE_DIR) { $env:ASTERIA_TEST_EXE_DIR } else { Join-Path $root 'build\windows\x64\Release' }
$exe = Join-Path $exeDir 'Asteria.exe'
if (-not (Test-Path $exe)) { throw "build first: $exe" }
$shots = Join-Path $exeDir 'Screenshots'
Remove-Item (Join-Path $shots 'horologium_*') -ErrorAction SilentlyContinue
$frames = 48
$steps = @('RUN~0.3', 'NOQUOTA=1~0.01', 'LEVEL=30~0.05', 'GOD=1~0.01')
foreach ($gate in 0, 1, 2) {
    $steps += @('MOBCLEAR~0.05', 'SPAWN=14~0.05', 'MOBHP=1000000~0.02')
    if ($gate -gt 0) { $steps += "MOBGATE=$gate~0.02" }
    1..$frames | ForEach-Object {
        $steps += 'HP=1~0.01'
        if ($_ % 8 -eq 3) { $steps += 'WALK=W~0.01' } elseif ($_ % 8 -eq 7) { $steps += 'WALK=S~0.01' }
        $steps += "S_${gate}_$($_)~0.25"
    }
    $steps += 'WALK=0~0.01'
}
$env:ASTERIA_SMOKE = $steps -join ';'
$env:ASTERIA_SMOKE_TAG = 'horologium'
$env:ASTERIA_SMOKE_SIZE = '1280x720'
$saveDir = Join-Path $env:TEMP 'asteria_smoke_horologium_save'
New-Item -ItemType Directory -Force $saveDir | Out-Null
$env:ASTERIA_SAVE_DIR = $saveDir
try {
    $p = Start-Process -FilePath $exe -WorkingDirectory $exeDir -PassThru -WindowStyle Hidden
    if (-not $p.WaitForExit(360000)) { $p.Kill(); throw 'horologium smoke timed out' }
    if ($p.ExitCode -ne 0) { throw "horologium smoke exited with $($p.ExitCode)" }
} finally {
    Remove-Item Env:ASTERIA_SMOKE, Env:ASTERIA_SMOKE_TAG, Env:ASTERIA_SMOKE_SIZE, Env:ASTERIA_SAVE_DIR -ErrorAction SilentlyContinue
}
$py = @'
import glob, sys
from PIL import Image
d, frames = sys.argv[1], int(sys.argv[2])
for gate in (0, 1, 2):
    picks = list(range(3, frames + 1, 3))
    cols = 4
    W, H = 480, 270
    sheet = Image.new('RGB', (W * cols, H * ((len(picks) + cols - 1) // cols)))
    for n, i in enumerate(picks):
        f = glob.glob(f'{d}/horologium_*_S_{gate}_{i}.png')
        if f: sheet.paste(Image.open(f[0]).convert('RGB').crop((240, 90, 1200, 630)).resize((W, H)), ((n % cols) * W, (n // cols) * H))
    sheet.save(f'{d}/horologium_phase{gate + 1}.png')
'@
$pyFile = Join-Path $env:TEMP 'horologium_sheet.py'
Set-Content -LiteralPath $pyFile -Value $py -Encoding utf8
& python $pyFile $shots $frames
Get-ChildItem (Join-Path $shots 'horologium_phase*.png') | ForEach-Object { Write-Host $_.FullName }
