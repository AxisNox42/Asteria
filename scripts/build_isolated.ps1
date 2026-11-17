# 별도 폴더 빌드 → build/isolated/<Tag>/Asteria.exe
# 기본 빌드(build_windows.ps1)가 실패할 때 쓰는 대체 빌드. 출력·중간 파일을 따로 두므로
#   - 게임이 켜져 있어 Release\Asteria.exe 가 잠긴 경우
#   - 다른 세션이 같은 폴더에서 동시에 빌드 중인 경우
# 와 겹치지 않는다. 결과 exe 로 테스트하려면 ASTERIA_TEST_EXE_DIR 에 출력 폴더를 넣는다:
#   $env:ASTERIA_TEST_EXE_DIR = (powershell -File scripts\build_isolated.ps1 -Tag lynx | Select-Object -Last 1)
#
# -FromHead -Files a,b : 다른 세션이 고치던 파일 때문에 컴파일이 깨질 때. HEAD 의 깨끗한 사본
#   (git worktree, build/isolated/<Tag>-src)에 지정한 파일만 지금 작업 폴더에서 덮어써 빌드한다.
# -Rebuild : 헤더 구조체 배치를 바꿨을 때 (crash-triage 참고)
# 마지막 줄에 exe 가 있는 폴더 경로를 출력한다.
param(
    [string]$Tag = "isolated",
    [switch]$Rebuild,
    [switch]$FromHead,
    [string[]]$Files = @()
)
$ErrorActionPreference = "Stop"
$Root = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
if ($Tag -notmatch '^[A-Za-z0-9_.-]+$') { throw "Tag 는 영문·숫자·_.- 만: $Tag" }
$Base = Join-Path $Root "build\isolated\$Tag"
$OutDir = Join-Path $Base "bin\"
$IntDir = Join-Path $Base "obj\"
$Src = $Root

if ($FromHead) {
    $Src = Join-Path $Root "build\isolated\$Tag-src"
    if (Test-Path $Src) {
        git -C $Root worktree remove --force $Src 2>$null
        if (Test-Path $Src) { Remove-Item -Recurse -Force $Src }
    }
    git -C $Root worktree prune
    git -C $Root worktree add --detach $Src HEAD | Out-Host
    if ($LASTEXITCODE -ne 0) { throw "worktree 생성 실패" }
    foreach ($f in $Files) {
        $from = Join-Path $Root $f
        $to = Join-Path $Src $f
        if (Test-Path $from) {
            New-Item -ItemType Directory -Force (Split-Path $to -Parent) | Out-Null
            Copy-Item -Force $from $to
        } elseif (Test-Path $to) {
            Remove-Item -Force $to   # 작업 폴더에서 지운 파일
        }
    }
}

$Vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$Found = if (Test-Path $Vswhere) {
    & $Vswhere -latest -products * -requires Microsoft.Component.MSBuild -find "MSBuild\**\Bin\MSBuild.exe" |
        Select-Object -First 1
}
$Msbuild = @(
    $Found,
    "${env:ProgramFiles}\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe",
    "${env:ProgramFiles}\Microsoft Visual Studio\2022\Professional\MSBuild\Current\Bin\MSBuild.exe",
    "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin\MSBuild.exe"
) | Where-Object { $_ -and (Test-Path $_) } | Select-Object -First 1
if (-not $Msbuild) { throw "MSBuild를 찾을 수 없습니다." }

$Vcx = Join-Path $Src "WiNILL\WiNILL.vcxproj"
$Target = if ($Rebuild) { "/t:Rebuild" } else { "/t:Build" }
& $Msbuild $Vcx $Target /p:Configuration=Release /p:Platform=x64 /p:PreferredToolArchitecture=x64 `
    "/p:OutDir=$OutDir" "/p:IntDir=$IntDir" /v:minimal | Out-Host
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

$Exe = Join-Path $OutDir "Asteria.exe"
if (-not (Test-Path $Exe)) { throw "빌드 결과가 없습니다: $Exe" }
Write-Host ""
Write-Host "별도 빌드: $Exe"
Write-Host "테스트할 때: `$env:ASTERIA_TEST_EXE_DIR = '$($OutDir.TrimEnd('\'))'"
$OutDir.TrimEnd('\')
