# Frame (weapon) balance measurement (Docs/Plans/2026-11-24_무기_밸런스_기획.docx).
# Every frame on its own, against the bare gun (BASE = 1.00):
#   single  one LYNX with 1,000,000 HP pinned -Dx px right of the ship (MOBPLACE every 0.25 s),
#           pointer on it. Regular mobs keep spawning meanwhile: piercing / area frames (laser,
#           crossbow, wave, mortar) also hit them, so their single score reads high (see the plan).
#   crowd   wave -Wave (mobs ~110 HP at wave 10; on wave 1 every frame one-shots them and only the
#           spawn rate shows), mobs laid out round the ship, fixed pointer, -Watch seconds.
#   overall sqrt(single * crowd).
# A run where a level-up handed out an active skill (its damage would mix in) is left out.
# Smoke runs open on virtual desktop 2, make no sound and keep the save in a temp folder.
#   powershell -File scripts\test_frame_balance.ps1 [-Runs 2] [-Codes SYN_F_SNIPER,...]
param(
    [string[]]$Codes = @('SYN_F_MINIGUN', 'SYN_F_LASER', 'SYN_F_SHOTGUN', 'SYN_F_SNIPER', 'SYN_F_BURST',
                         'SYN_F_CROSSBOW', 'SYN_F_WAVE', 'SYN_F_MORTAR'),
    [int]$Runs = 2,
    [int]$Wave = 10,
    [double]$Watch = 8.0,
    [int]$Dx = 300,
    [string]$Out = ''
)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$exeDir = if ($env:ASTERIA_TEST_EXE_DIR) { $env:ASTERIA_TEST_EXE_DIR } else { Join-Path $root 'build\windows\x64\Release' }
$exe = Join-Path $exeDir 'Asteria.exe'
if (-not (Test-Path $exe)) { throw "build first: $exe" }
$shots = Join-Path $exeDir 'Screenshots'
$stateFile = Join-Path $shots 'skill_state.txt'
if (-not $Out) { $Out = Join-Path $root ('Docs\QA\' + (Get-Date -Format 'yyyy-MM-dd') + '_무기밸런스') }
New-Item -ItemType Directory -Force $Out | Out-Null
$saveDir = Join-Path $env:TEMP 'asteria_frame_balance_save'
New-Item -ItemType Directory -Force $saveDir | Out-Null
$Codes = @('BASE') + @($Codes | ForEach-Object { $_ -split ',' } | Where-Object { $_ -and $_ -ne 'BASE' })
$tag = 'fbal'

# The LYNX sits at screen x 0.5 + Dx * 0.46 / 1280 (smoke window 1280x720, play zoom ~0.46).
$singleAim = '@{0:F3},0.5' -f (0.5 + $Dx * 0.46 / 1280.0)
$crowdAim = '@0.60,0.42'

function Single-Steps([string]$code) {
    $a = $singleAim
    $s = "RUN~0.3;GOD=1~0.05;NOQUOTA=1~0.05;NOGUN=0~0.05;MOBCLEAR~0.1"
    if ($code -ne 'BASE') { $s += ";GIVE=$code$a~0.05" }
    $s += ";MOBCLEAR~0.05;SPAWN=11~0.05;MOBHP=1000000~0.05;MOBPLACE=$Dx$a~0.3"
    $s += ";SKILLDUMP=S-$code#0$a~0.02;FIRE=1$a~0.05"
    for ($i = 0; $i -lt [int][Math]::Round($Watch / 0.25); $i++) { $s += ";MOBPLACE=$Dx$a~0.25" }
    return $s + ";SKILLDUMP=S-$code#1$a~0.02;FIRE=0~0.05"
}
function Crowd-Steps([string]$code) {
    $a = $crowdAim
    $s = "RUN~0.3;WAVE=$Wave~0.05;GOD=1~0.05;NOQUOTA=1~0.05;NOGUN=0~0.05;MOBCLEAR~0.05"
    foreach ($k in 0..3) { $s += ';SPAWN=2~0.05' }
    $s += ';SPAWN=0~0.05;MOBGRID~0.05'
    $s += ";SKILLDUMP=C-$code#0~0.05"
    if ($code -ne 'BASE') { $s += ";GIVE=$code$a~0.05" }
    $s += ";FIRE=1$a~$Watch;SKILLDUMP=C-$code#1$a~0.02;FIRE=0~0.05"
    return $s
}

$all = @()
for ($r = 1; $r -le $Runs; $r++) {
    foreach ($kind in 'S', 'C') {
        for ($b = 0; $b -lt $Codes.Count; $b += 5) {
            $batch = $Codes[$b..([Math]::Min($b + 5, $Codes.Count) - 1)]
            Remove-Item -LiteralPath $stateFile -ErrorAction SilentlyContinue
            $env:ASTERIA_SMOKE = ($batch | ForEach-Object { if ($kind -eq 'S') { Single-Steps $_ } else { Crowd-Steps $_ } }) -join ';'
            $env:ASTERIA_SMOKE_TAG = $tag
            $env:ASTERIA_SMOKE_SIZE = '1280x720'
            $env:ASTERIA_SAVE_DIR = $saveDir
            try {
                $p = Start-Process -FilePath $exe -WorkingDirectory $exeDir -PassThru -WindowStyle Hidden
                if (-not $p.WaitForExit(300000)) { $p.Kill(); Write-Host "timeout: $($batch -join ',')" }
            } finally {
                Remove-Item Env:ASTERIA_SMOKE, Env:ASTERIA_SMOKE_TAG, Env:ASTERIA_SMOKE_SIZE, Env:ASTERIA_SAVE_DIR -ErrorAction SilentlyContinue
            }
            Get-ChildItem $shots -Filter "${tag}_*.png" -ErrorAction SilentlyContinue | Remove-Item
            if (-not (Test-Path $stateFile)) { Write-Host "no state: $($batch -join ',')"; continue }
            $lines = Get-Content $stateFile
            $lines | Add-Content -Path (Join-Path $Out 'state.txt') -Encoding utf8
            $rows = @{}
            foreach ($line in $lines) {
                $parts = $line.TrimStart([char]0xFEFF) -split ' '
                $label, $mark = $parts[0] -split '#'
                $kv = @{}; foreach ($q in $parts[1..99]) { if ($q -match '=') { $k, $v = $q -split '=', 2; $kv[$k] = $v } }
                if (-not $rows[$label]) { $rows[$label] = @{} }
                $rows[$label][$mark] = $kv
            }
            foreach ($code in $batch) {
                $x = $rows["$kind-$code"]
                if (-not $x -or -not $x['0'] -or -not $x['1']) { continue }
                $dealt = [double]$x['1'].dealt - [double]$x['0'].dealt
                $time = [Math]::Max(0.01, [double]$x['1'].time - [double]$x['0'].time)
                $skill = $x['1'].slot0 -ne '-:0.00'
                $all += [pscustomobject]@{ run = $r; kind = $kind; code = $code; dps = $dealt / $time
                                           kills = [int]$x['1'].kills - [int]$x['0'].kills; skill = $skill }
            }
        }
    }
    Write-Host "run $r done"
}
$all | Export-Csv (Join-Path $Out 'samples.csv') -NoTypeInformation -Encoding utf8
$clean = $all | Where-Object { -not $_.skill }
function Avg([string]$kind, [string]$code, [string]$field) {
    $v = @($clean | Where-Object { $_.kind -eq $kind -and $_.code -eq $code } | ForEach-Object { $_.$field })
    if ($v.Count -eq 0) { return $null }
    return ($v | Measure-Object -Average).Average
}
$bs = Avg 'S' 'BASE' 'dps'; $bc = Avg 'C' 'BASE' 'dps'
$table = foreach ($code in $Codes) {
    $s = Avg 'S' $code 'dps'; $c = Avg 'C' $code 'dps'
    if ($s -eq $null -or $c -eq $null -or -not $bs -or -not $bc) { [pscustomobject]@{ code = $code; single = 'NA' }; continue }
    $sr = $s / $bs; $cr = $c / $bc
    [pscustomobject]@{
        code = $code
        single = [Math]::Round($sr, 2)
        crowd = [Math]::Round($cr, 2)
        overall = [Math]::Round([Math]::Sqrt($sr * $cr), 2)
        crowdKills = [Math]::Round((Avg 'C' $code 'kills'), 1)
        samples = @($clean | Where-Object { $_.code -eq $code }).Count
    }
}
$table | Export-Csv (Join-Path $Out 'summary.csv') -NoTypeInformation -Encoding utf8
$table | Format-Table -AutoSize | Out-String -Width 200 | Write-Host
$dropped = @($all | Where-Object { $_.skill }).Count
if ($dropped) { Write-Host "$dropped sample(s) left out: a level-up gave an active skill" }
Write-Host "Out: $Out"
