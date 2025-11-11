<#
  vcpkg manifest 환경 부트스트랩 스크립트
  - vcpkg 루트(VCPKG_ROOT or 인자)를 확인
  - 선택적으로 builtin-baseline을 최신 해시로 주입
  - 지정한 triplet (기본: Windows x64-windows / Linux x64-linux) 의존성을 설치
  - 변경 사항은 사용자가 커밋해야 하므로 build.ps1은 manifest를 건드리지 않음
#>
[CmdletBinding()]
param(
  [string]$VcpkgRoot = "",
  [string]$Triplet = "",
  [switch]$SetBaseline,
  [string]$Baseline = ""
)

$ErrorActionPreference = 'Stop'
function Info($m){ Write-Host "[info] $m" -ForegroundColor Cyan }
function Warn($m){ Write-Host "[warn] $m" -ForegroundColor Yellow }
function Fail($m){ Write-Host "[fail] $m" -ForegroundColor Red; exit 1 }

if (-not $VcpkgRoot -or $VcpkgRoot -eq '') {
  if ($env:VCPKG_ROOT) { $VcpkgRoot = $env:VCPKG_ROOT }
  else {
    $probe = "C:/Program Files/Microsoft Visual Studio/2022/Community/VC/vcpkg"
    if (Test-Path $probe) { $VcpkgRoot = $probe }
  }
}
if (-not $VcpkgRoot -or -not (Test-Path $VcpkgRoot)) {
  Fail "VCPKG_ROOT를 찾을 수 없습니다. -VcpkgRoot 인자나 VCPKG_ROOT 환경 변수를 설정하세요."
}

if (-not $Triplet -or $Triplet -eq '') {
  if ($IsWindows) { $Triplet = 'x64-windows' }
  else { $Triplet = 'x64-linux' }
}
Info "VCPKG_ROOT=$VcpkgRoot"
Info "Triplet=$Triplet"

$vcpkgExe = Join-Path $VcpkgRoot $(if ($IsWindows) { 'vcpkg.exe' } else { 'vcpkg' })
if (-not (Test-Path $vcpkgExe)) { Fail "vcpkg 실행 파일을 찾을 수 없습니다: $vcpkgExe" }

$manifestPath = Join-Path (Resolve-Path .) 'vcpkg.json'
if (-not (Test-Path $manifestPath)) {
  Warn "vcpkg.json을 현재 디렉터리에서 찾지 못했습니다. manifest 모드가 아니라면 직접 vcpkg install을 실행하세요."
} elseif ($SetBaseline) {
  if ($Baseline -and $Baseline.Length -ge 8) {
    Info "baseline 인자로 전달된 값 사용: $Baseline"
  } else {
    Info "git ls-remote https://github.com/microsoft/vcpkg HEAD 호출"
    try {
      $ls = git ls-remote https://github.com/microsoft/vcpkg HEAD 2>$null
      if ($ls) { $Baseline = ($ls -split "\s+")[0] }
    } catch {
      Warn "원격 해시를 가져오지 못했습니다: $_"
    }
  }
  if ($Baseline -and $Baseline.Length -ge 8) {
    try {
      $json = Get-Content -Raw $manifestPath | ConvertFrom-Json
      $json | Add-Member -NotePropertyName 'builtin-baseline' -NotePropertyValue $Baseline -Force
      ($json | ConvertTo-Json -Depth 6) | Set-Content -Encoding UTF8 $manifestPath
      Info "builtin-baseline=$Baseline 적용 완료 (수정된 manifest를 커밋하세요)"
    } catch {
      Warn "vcpkg.json baseline 갱신 실패: $_"
    }
  } else {
    Warn "유효한 baseline 해시를 확보하지 못했습니다."
  }
}

Info "vcpkg install --triplet $Triplet 실행"
& $vcpkgExe install --triplet $Triplet
if ($LASTEXITCODE -ne 0) { Fail "vcpkg install 실패" }

Info "완료"
