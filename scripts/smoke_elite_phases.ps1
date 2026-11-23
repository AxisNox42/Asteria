# Elite phase patterns (Docs/Plans/2026-11-17_엘리트_페이즈패턴.md): each elite
# alone, put past one gate (phase 2) and then two (phase 3), captured every
# 0.3 s. Writes Screenshots\elitephase_<kind>.png, a sheet per elite
# (row 1 = phase 2, row 2 = phase 3). Silent, like every smoke run.
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$exeDir = if ($env:ASTERIA_TEST_EXE_DIR) { $env:ASTERIA_TEST_EXE_DIR } else { Join-Path $root 'build\windows\x64\Release' }
$exe = Join-Path $exeDir 'Asteria.exe'
if (-not (Test-Path $exe)) { throw "build first: $exe" }
$shots = Join-Path $exeDir 'Screenshots'
Remove-Item (Join-Path $shots 'elitephase_*') -ErrorAction SilentlyContinue
$kinds = @{ '4' = 'quasar'; '7' = 'antares'; '1' = 'genesis'; '6' = 'regulus' }
$frames = 24
$steps = @('RUN~0.3', 'NOQUOTA=1~0.01', 'LEVEL=30~0.05', 'GOD=1~0.01', 'WALK=D~0.9', 'WALK=0~0.05')
foreach ($k in '4', '7', '1', '6') {
    foreach ($gate in 1, 2) {
        $steps += @('MOBCLEAR~0.05', "SPAWN=$k~0.05")
        if ($k -eq '6') { 1..4 | ForEach-Object { $steps += 'SPAWN=0~0.02' } }
        $steps += @('MOBHP=1000000~0.02', "MOBGATE=$gate~0.02")
        1..$frames | ForEach-Object { $steps += 'HP=1~0.01'; $steps += "S_${k}_${gate}_$($_)~0.3" }
    }
}
$env:ASTERIA_SMOKE = $steps -join ';'
$env:ASTERIA_SMOKE_TAG = 'elitephase'
$env:ASTERIA_SMOKE_SIZE = '1280x720'
try {
    $p = Start-Process -FilePath $exe -WorkingDirectory $exeDir -PassThru -WindowStyle Hidden
    if (-not $p.WaitForExit(420000)) { $p.Kill(); throw 'elite phase smoke timed out' }
    if ($p.ExitCode -ne 0) { throw "elite phase smoke exited with $($p.ExitCode)" }
} finally {
    Remove-Item Env:ASTERIA_SMOKE, Env:ASTERIA_SMOKE_TAG, Env:ASTERIA_SMOKE_SIZE -ErrorAction SilentlyContinue
}
$py = @'
import glob, sys
from PIL import Image
d, k, name, frames = sys.argv[1], sys.argv[2], sys.argv[3], int(sys.argv[4])
pick = list(range(2, frames + 1, 3))          # every third capture
W, H = 320, 180
sheet = Image.new('RGB', (W * len(pick), H * 2))
for r, gate in enumerate((1, 2)):
    for c, i in enumerate(pick):
        f = glob.glob(f'{d}/elitephase_KR_*_S_{k}_{gate}_{i}.png')
        if f: sheet.paste(Image.open(f[0]).convert('RGB').resize((W, H)), (c * W, r * H))
sheet.save(f'{d}/elitephase_{name}.png')
'@
$pyFile = Join-Path $env:TEMP 'elitephase_sheet.py'
Set-Content -LiteralPath $pyFile -Value $py -Encoding utf8
foreach ($k in $kinds.Keys) { & python $pyFile $shots $k $kinds[$k] $frames }
Get-ChildItem (Join-Path $shots 'elitephase_*.png') | Where-Object { $_.Name -notmatch '_KR_' } | ForEach-Object { Write-Host $_.FullName }
