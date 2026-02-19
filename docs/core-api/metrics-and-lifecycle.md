# Metrics and Lifecycle API Guide

## Stability

| Header | Stability |
|---|---|
| `server/core/app/app_host.hpp` | `[Stable]` |
| `server/core/metrics/http_server.hpp` | `[Stable]` |
| `server/core/metrics/build_info.hpp` | `[Stable]` |
| `server/core/runtime_metrics.hpp` | `[Transitional]` |

## Lifecycle Contract
- `AppHost` lifecycle phase: `init -> bootstrapping -> running -> stopping -> stopped|failed`.
- Readiness and health are separate from lifecycle phase and can be reported independently.

## Metrics Contract
- `MetricsHttpServer` exposes `/metrics`, `/healthz`, `/readyz`, and optional custom routes.
- `build_info` helper emits `knights_build_info` in Prometheus text format.
- `runtime_metrics` counters are transitional and may be normalized further.

## Operational Rule
- Keep metrics callbacks lightweight and non-blocking.
