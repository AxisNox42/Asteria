# Nightmare REGULUS royal wave, look pass (Docs/Plans/2026-11-23_레굴루스_난이도별_세부.md §8):
# a Regulus with six Rotor allies on NIGHTMARE. Segment 0 starts in phase 1 and is
# moved to phase 2 (the laned ring), then runs its phase-2 fans; segment 1 is
# phase 3 (wider fan, two bands). Captured every 0.1 s.
# Writes Screenshots\reguluswave_seg<n>_<part>.png. Silent; desktop 2.
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$exeDir = if ($env:ASTERIA_TEST_EXE_DIR) { $env:ASTERIA_TEST_EXE_DIR } else { Join-Path $root 'build\windows\x64\Release' }
$exe = Join-Path $exeDir 'Asteria.exe'
if (-not (Test-Path $exe)) { throw "build first: $exe" }
$shots = Join-Path $exeDir 'Screenshots'
Remove-Item (Join-Path $shots 'reguluswave_*') -ErrorAction SilentlyContinue
$frames = 120
$steps = @('DIFF=NIGHTMARE~0.01', 'RUN~0.3', 'DIFF=NIGHTMARE~0.01', 'NOQUOTA=1~0.01', 'LEVEL=30~0.05', 'GOD=1~0.01')
foreach ($seg in 0, 1) {
    $steps += @('MOBCLEAR~0.05', 'SPAWN=6~0.05')
    1..6 | ForEach-Object { $steps += 'SPAWN=0~0.02' }
    $steps += 'MOBHP=1000000~0.02'
    if ($seg -eq 1) { $steps += 'MOBGATE=2~0.02' }
    $steps += 'MOBPLACE=450~0.02'
    1..$frames | ForEach-Object {
        $steps += 'HP=1~0.01'
        if ($seg -eq 0 -and $_ -eq 6) { $steps += 'MOBGATE=1~0.01' }   # phase change: the ring
        if ($_ % 20 -eq 1) { $steps += 'WALK=U~0.01' } elseif ($_ % 20 -eq 11) { $steps += 'WALK=D~0.01' }
        $steps += "S_${seg}_$($_)~0.1"
    }
    $steps += 'WALK=0~0.01'
}
$env:ASTERIA_SMOKE = $steps -join ';'
$env:ASTERIA_SMOKE_TAG = 'reguluswave'
$env:ASTERIA_SMOKE_SIZE = '1280x720'
$saveDir = Join-Path $env:TEMP 'asteria_smoke_reguluswave_save'
New-Item -ItemType Directory -Force $saveDir | Out-Null
$env:ASTERIA_SAVE_DIR = $saveDir
try {
    $p = Start-Process -FilePath $exe -WorkingDirectory $exeDir -PassThru -WindowStyle Hidden
    if (-not $p.WaitForExit(360000)) { $p.Kill(); throw 'regulus wave smoke timed out' }
    if ($p.ExitCode -ne 0) { throw "regulus wave smoke exited with $($p.ExitCode)" }
} finally {
    Remove-Item Env:ASTERIA_SMOKE, Env:ASTERIA_SMOKE_TAG, Env:ASTERIA_SMOKE_SIZE, Env:ASTERIA_SAVE_DIR -ErrorAction SilentlyContinue
}
$py = @'
import glob, sys
from PIL import Image
d, frames = sys.argv[1], int(sys.argv[2])
cols, W, H, per = 6, 400, 225, 30
for seg in (0, 1):
    for part in range(0, frames, per):
        picks = list(range(part + 1, min(frames, part + per) + 1))
        sheet = Image.new('RGB', (W * cols, H * ((len(picks) + cols - 1) // cols)))
        for n, i in enumerate(picks):
            f = glob.glob(f'{d}/reguluswave_*_S_{seg}_{i}.png')
            if f: sheet.paste(Image.open(f[0]).convert('RGB').resize((W, H)), ((n % cols) * W, (n // cols) * H))
        sheet.save(f'{d}/reguluswave_seg{seg}_{part // per}.png')
'@
$pyFile = Join-Path $env:TEMP 'reguluswave_sheet.py'
Set-Content -LiteralPath $pyFile -Value $py -Encoding utf8
& python $pyFile $shots $frames
Get-ChildItem (Join-Path $shots 'reguluswave_seg*.png') | ForEach-Object { Write-Host $_.FullName }
