# 테스트용 Windows 빌드 → build/windows/x64/Release/WiNILL.exe
# -Rebuild: 헤더의 구조체 배치를 바꾼 뒤에는 반드시 사용 (증분 빌드가 낡은 .obj 를 남겨 크래시를 만든다)
param([switch]$Rebuild)
$ErrorActionPreference = "Stop"
$Root = Resolve-Path (Join-Path $PSScriptRoot "..")
$Vcx = Join-Path $Root "WiNILL\WiNILL.vcxproj"
$Out = Join-Path $Root "build\windows\x64\Release\WiNILL.exe"
$IntDir = Join-Path $Root "build\windows\obj\WiNILL\x64\Release\"

# vswhere 로 설치 위치와 무관하게 찾고 (D:\ 등 사용자 지정 경로 포함), 없으면 기본 경로를 본다
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

$Target = if ($Rebuild) { "/t:Rebuild" } else { "/t:Build" }
& $Msbuild $Vcx $Target /p:Configuration=Release /p:Platform=x64 /p:PreferredToolArchitecture=x64 "/p:IntDir=$IntDir" /v:minimal
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host ""
Write-Host "테스트용: $Out"
