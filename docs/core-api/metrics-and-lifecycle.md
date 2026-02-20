# Metrics and Lifecycle API Guide

## Stability

| Header | Stability |
|---|---|
| `server/core/app/app_host.hpp` | `[Stable]` |
| `server/core/app/termination_signals.hpp` | `[Stable]` |
| `server/core/metrics/metrics.hpp` | `[Stable]` |
| `server/core/metrics/http_server.hpp` | `[Stable]` |
| `server/core/metrics/build_info.hpp` | `[Stable]` |
| `server/core/runtime_metrics.hpp` | `[Stable]` |

## Lifecycle Contract
- `AppHost` lifecycle phase: `init -> bootstrapping -> running -> stopping -> stopped|failed`.
- `termination_signals` exposes process-global, async-signal-safe stop intent polling via `sig_atomic_t` flag.
- Readiness and health are separate from lifecycle phase and can be reported independently.

## Metrics Contract
- `metrics` API (`get_counter/get_gauge/get_histogram`) guarantees no-op-safe fallback when exporter backend is absent.
- `MetricsHttpServer` exposes `/metrics`, `/healthz`, `/readyz`, and optional custom routes.
- `build_info` helper emits `knights_build_info` in Prometheus text format.
- `runtime_metrics` snapshot exposes process-wide counters and histogram buckets as stable read contract.

## Operational Rule
- Keep metrics callbacks lightweight and non-blocking.
