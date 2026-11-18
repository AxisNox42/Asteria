# Skill-by-skill smoke (Docs/Plans/2026-11-18_스킬_전수검증.md): every card on its own
# in a fresh run, captured a few times after it is taken, with a damage / state line
# per card in Screenshots/skill_state.txt.
#   -Codes  AugDef::name list (A_*, SYN codes...). Default: every active (A_*).
#   -NoGun  the gun stays quiet (FIRE=1 still casts skills), so the damage is the skill's.
#   -Baseline  also run one card-less pass first (label BASE) for comparison.
#   -Out    where the PNGs and skill_state.txt are copied (one folder per card).
# Smoke runs open on virtual desktop 2, make no sound and skip SaveGame.
param(
    [string[]]$Codes = @(),
    [switch]$NoGun,
    [switch]$Baseline,
    [int]$PerProcess = 6,
    [string]$Out = '',
    [string]$Tag = 'skill',
    [double]$Watch = 5.0
)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$exeDir = if ($env:ASTERIA_TEST_EXE_DIR) { $env:ASTERIA_TEST_EXE_DIR } else { Join-Path $root 'build\windows\x64\Release' }
$exe = Join-Path $exeDir 'Asteria.exe'
if (-not (Test-Path $exe)) { throw "build first: $exe" }
$shots = Join-Path $exeDir 'Screenshots'
$stateFile = Join-Path $shots 'skill_state.txt'
if (-not $Out) { $Out = Join-Path $root 'Docs\QA\2026-11-18_스킬검증\shots' }
New-Item -ItemType Directory -Force $Out | Out-Null

$Codes = @($Codes | ForEach-Object { $_ -split ',' } | Where-Object { $_ })
if ($Codes.Count -eq 0) {
    $header = Get-Content (Join-Path $root 'WiNILL\Entity\Augment.h') -Raw
    $Codes = [regex]::Matches($header, '\bA_[A-Z_]+\b') | ForEach-Object { $_.Value } | Select-Object -Unique
}
$list = @()
if ($Baseline) { $list += 'BASE' }
$list += $Codes

$saveDir = Join-Path $env:TEMP 'asteria_skillqa_save'
New-Item -ItemType Directory -Force $saveDir | Out-Null

# Capture points after the card is taken (seconds from the pick).
$marks = @(0.4, 1.2, 2.5, $Watch)
$gun = if ($NoGun) { '1' } else { '0' }
# Every step from the pick on keeps the pointer here: a skill that is ready casts the frame it
# is taken, aimed at the pointer (off-screen without one). Right of the ship, on the mob grid.
$aim = '@0.60,0.42'

function Steps-For([string]$code) {
    $s = "RUN~0.3;GOD=1~0.05;NOQUOTA=1~0.05;NOGUN=$gun~0.05"
    foreach ($k in 0..3) { $s += ';SPAWN=2~0.05' }
    $s += ';SPAWN=0~0.05;MOBGRID~0.05'
    $s += ";SKILLDUMP=$code#0~0.05"
    if ($code -ne 'BASE') { foreach ($g in $code -split '\+') { $s += ";GIVE=$g$aim~0.05" } }   # A+B: several cards together
    $prev = 0.0
    $i = 1
    foreach ($m in $marks) {
        $hold = [Math]::Round($m - $prev, 2)
        $step = if ($i -eq 1) { 'FIRE=1' } else { "X-$code-$i" }
        $s += ";$step$aim~$hold"
        $s += ";SKILLDUMP=$code#$i$aim~0.02"
        $prev = $m
        $i++
    }
    return $s
}

Add-Type -AssemblyName System.Drawing
# The raw captures are ~3.6 MB each; keep a JPEG copy instead.
function Save-Jpg([string]$src, [string]$dst) {
    $img = [System.Drawing.Image]::FromFile($src)
    try {
        $codec = [System.Drawing.Imaging.ImageCodecInfo]::GetImageEncoders() | Where-Object { $_.MimeType -eq 'image/jpeg' }
        $ep = New-Object System.Drawing.Imaging.EncoderParameters 1
        $ep.Param[0] = New-Object System.Drawing.Imaging.EncoderParameter ([System.Drawing.Imaging.Encoder]::Quality, [long]88)
        $img.Save($dst, $codec, $ep)
    } finally { $img.Dispose() }
}

$fail = @()
for ($b = 0; $b -lt $list.Count; $b += $PerProcess) {
    $batch = $list[$b..([Math]::Min($b + $PerProcess, $list.Count) - 1)]
    Remove-Item -LiteralPath $stateFile -ErrorAction SilentlyContinue
    Get-ChildItem $shots -Filter "${Tag}_*.png" -ErrorAction SilentlyContinue | Remove-Item
    $env:ASTERIA_SMOKE = ($batch | ForEach-Object { Steps-For $_ }) -join ';'
    $env:ASTERIA_SMOKE_TAG = $Tag
    $env:ASTERIA_SMOKE_SIZE = '1280x720'
    $env:ASTERIA_SAVE_DIR = $saveDir
    try {
        $p = Start-Process -FilePath $exe -WorkingDirectory $exeDir -PassThru -WindowStyle Hidden
        if (-not $p.WaitForExit(240000)) { $p.Kill(); $fail += "$($batch -join ',') timeout" }
        elseif ($p.ExitCode -ne 0) { $fail += "$($batch -join ',') exit $($p.ExitCode)" }
    } finally {
        Remove-Item Env:ASTERIA_SMOKE, Env:ASTERIA_SMOKE_TAG, Env:ASTERIA_SMOKE_SIZE, Env:ASTERIA_SAVE_DIR -ErrorAction SilentlyContinue
    }
    # PNGs come out in step order; hand each card the shots of its own steps.
    $pngs = Get-ChildItem $shots -Filter "${Tag}_*.png" -ErrorAction SilentlyContinue | Sort-Object { [int](($_.BaseName -split '_')[2]) }
    foreach ($code in $batch) {
        $dir = Join-Path $Out $code
        New-Item -ItemType Directory -Force $dir | Out-Null
        $mine = $pngs | Where-Object { $_.Name -match "FIRE|X-$([regex]::Escape($code))-" }
        $n = 1
        foreach ($f in ($pngs | Where-Object { $_.Name -match "_X-$([regex]::Escape($code))-" })) {
            $n++; Save-Jpg $f.FullName (Join-Path $dir "t$n.jpg")
        }
    }
    # The FIRE=1 shots (t1) are in order, one per card.
    $fires = @($pngs | Where-Object { $_.Name -match '_FIRE' })
    for ($k = 0; $k -lt $batch.Count -and $k -lt $fires.Count; $k++) {
        Save-Jpg $fires[$k].FullName (Join-Path (Join-Path $Out $batch[$k]) 't1.jpg')
    }
    if (Test-Path $stateFile) {
        Get-Content $stateFile | Add-Content -Path (Join-Path $Out 'skill_state.txt') -Encoding utf8
    } else { $fail += "$($batch -join ',') no state" }
    Write-Host "batch $($b / $PerProcess + 1): $($batch -join ', ')"
}

# Summary: damage and kills over the watch window per card.
$rows = @{}
foreach ($line in Get-Content (Join-Path $Out 'skill_state.txt')) {
    $label = ($line -split ' ')[0]
    $code, $mark = $label -split '#'
    $kv = @{}
    foreach ($p in ($line -split ' ')[1..99]) { if ($p -match '=') { $k, $v = $p -split '=', 2; $kv[$k] = $v } }
    if (-not $rows[$code]) { $rows[$code] = @{} }
    $rows[$code][$mark] = $kv
}
$summary = foreach ($code in $list) {
    $r = $rows[$code]
    if (-not $r -or -not $r['0'] -or -not $r['4']) { [pscustomobject]@{ code = $code; dealt = 'NA' }; continue }
    [pscustomobject]@{
        code  = $code
        dealt = [Math]::Round([double]$r['4'].dealt - [double]$r['0'].dealt, 1)
        kills = [int]$r['4'].kills - [int]$r['0'].kills
        slot0 = $r['4'].slot0
        shield = $r['4'].shield
        maxhp = $r['4'].maxhp
    }
}
$summary | Export-Csv (Join-Path $Out 'summary.csv') -NoTypeInformation -Encoding utf8 -Append
$summary | Format-Table -AutoSize | Out-String -Width 200 | Write-Host
if ($fail.Count) { Write-Host "FAILURES:"; $fail | ForEach-Object { Write-Host "  $_" } }




