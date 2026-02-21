param(
    [string]$CanaryEnvFile = "docker/stack/.env.udp-canary.example",
    [string]$RollbackEnvFile = "docker/stack/.env.udp-rollback.example",
    [string]$ProjectName = "knights-stack",
    [switch]$Observability = $true,
    [switch]$Build = $false
)

$ErrorActionPreference = "Stop"

try { chcp 65001 | Out-Null } catch {}

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Definition
$ProjectRoot = Resolve-Path (Join-Path $ScriptDir "..")
Set-Location $ProjectRoot

function Info([string]$Message) {
    Write-Host "[info] $Message" -ForegroundColor Cyan
}

function Invoke-Deploy([string]$Action, [string]$EnvFile, [switch]$Detached) {
    $deployScript = Join-Path $ScriptDir "deploy_docker.ps1"
    $args = @(
        "-NoProfile",
        "-ExecutionPolicy", "Bypass",
        "-File", $deployScript,
        "-Action", $Action,
        "-ProjectName", $ProjectName
    )

    if ($EnvFile -and $EnvFile.Trim() -ne "") {
        $args += @("-EnvFile", $EnvFile)
    }
    if ($Observability) {
        $args += "-Observability"
    }
    if ($Build) {
        $args += "-Build"
    }
    if ($Detached) {
        $args += "-Detached"
    }

    pwsh @args
}

function Wait-EndpointReady([string]$Url, [int]$TimeoutSec = 60) {
    $deadline = (Get-Date).AddSeconds($TimeoutSec)
    while ((Get-Date) -lt $deadline) {
        try {
            $resp = Invoke-WebRequest -Uri $Url -TimeoutSec 5
            if ($resp.StatusCode -eq 200) {
                return
            }
        } catch {
            Start-Sleep -Milliseconds 500
        }
    }
    throw "Endpoint not ready: $Url"
}

function Get-MetricValue([string]$MetricsText, [string]$MetricName) {
    $pattern = "(?m)^" + [regex]::Escape($MetricName) + "\\s+([0-9]+(?:\\.[0-9]+)?)$"
    $m = [regex]::Match($MetricsText, $pattern)
    if (-not $m.Success) {
        throw "Metric not found: $MetricName"
    }
    return [double]::Parse($m.Groups[1].Value, [System.Globalization.CultureInfo]::InvariantCulture)
}

function Assert-GatewayUdpState([string]$MetricsUrl, [double]$ExpectedEnabled) {
    Wait-EndpointReady $MetricsUrl 60
    $resp = Invoke-WebRequest -Uri $MetricsUrl -TimeoutSec 5
    $metrics = $resp.Content

    $feature = Get-MetricValue $metrics "gateway_udp_ingress_feature_enabled"
    if ($feature -ne 1) {
        throw "Gateway UDP ingress feature build flag is off at $MetricsUrl"
    }

    $enabled = Get-MetricValue $metrics "gateway_udp_enabled"
    if ($enabled -ne $ExpectedEnabled) {
        throw "Unexpected gateway_udp_enabled at $MetricsUrl (expected=$ExpectedEnabled actual=$enabled)"
    }
}

$sw = [System.Diagnostics.Stopwatch]::StartNew()

Info "Starting canary rollout rehearsal"
Invoke-Deploy -Action "down" -EnvFile $CanaryEnvFile
Invoke-Deploy -Action "up" -EnvFile $CanaryEnvFile -Detached

Assert-GatewayUdpState "http://127.0.0.1:36001/metrics" 1
Assert-GatewayUdpState "http://127.0.0.1:36002/metrics" 0

Info "Running TCP smoke on canary stage"
python tests/python/verify_pong.py

Info "Executing rollback to TCP-only"
Invoke-Deploy -Action "down" -EnvFile $CanaryEnvFile
Invoke-Deploy -Action "up" -EnvFile $RollbackEnvFile -Detached

Assert-GatewayUdpState "http://127.0.0.1:36001/metrics" 0
Assert-GatewayUdpState "http://127.0.0.1:36002/metrics" 0

Info "Running TCP smoke after rollback"
python tests/python/verify_pong.py

$sw.Stop()
$elapsedSec = [math]::Round($sw.Elapsed.TotalSeconds, 2)

if ($sw.Elapsed.TotalMinutes -gt 10) {
    throw "Rollback rehearsal exceeded 10 minutes ($elapsedSec sec)"
}

Info "Rollback rehearsal completed in $elapsedSec sec (<= 10 min)"
