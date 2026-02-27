<#
  Validate Prometheus alert rules using promtool in Docker.

  - Syntax check: docker/observability/prometheus/alerts.yml
  - Rule unit test: docker/observability/prometheus/alerts.tests.yml
#>

[CmdletBinding()]
param(
  [string]$PrometheusImage = "prom/prometheus:v3.2.1"
)

$ErrorActionPreference = 'Stop'

function Info([string]$Message) { Write-Host "[info] $Message" -ForegroundColor Cyan }
function Fail([string]$Message) { Write-Host "[fail] $Message" -ForegroundColor Red; exit 1 }

if (-not (Get-Command docker -ErrorAction SilentlyContinue)) {
  Fail "docker command not found. Install Docker Desktop or run checks in CI Linux runner."
}

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$mount = "${repoRoot}:/work"

Info "Checking Prometheus rule syntax"
docker run --rm --entrypoint promtool -v "$mount" -w /work $PrometheusImage check rules docker/observability/prometheus/alerts.yml
if ($LASTEXITCODE -ne 0) {
  exit $LASTEXITCODE
}

Info "Running Prometheus rule tests"
docker run --rm --entrypoint promtool -v "$mount" -w /work $PrometheusImage test rules docker/observability/prometheus/alerts.tests.yml
if ($LASTEXITCODE -ne 0) {
  exit $LASTEXITCODE
}

Info "Prometheus alert rules look good"
