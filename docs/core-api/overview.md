# server_core API Overview

## Status Legend
- `[Stable]`: Public contract with compatibility promise.
- `[Transitional]`: Public today, still being hardened.
- `[Internal]`: Not for public/sample usage.

## Module Map

| Module | Stability | Primary Headers | Purpose |
|---|---|---|---|
| Runtime Host | `[Stable]` | `server/core/app/app_host.hpp` | Process lifecycle, readiness/health, admin HTTP integration. |
| Networking | `[Stable]+[Transitional]` | `server/core/net/hive.hpp`, `server/core/net/dispatcher.hpp`, `server/core/net/listener.hpp`, `server/core/net/connection.hpp`, `server/core/net/acceptor.hpp`, `server/core/net/session.hpp` | Event loop lifecycle + transport/session routing primitives. |
| Concurrency | `[Stable]+[Transitional]+[Internal]` | `server/core/concurrent/task_scheduler.hpp`, `server/core/concurrent/job_queue.hpp`, `server/core/concurrent/thread_manager.hpp` | Scheduler and worker queue primitives. |
| Storage SPI | `[Transitional]` | `server/core/storage/connection_pool.hpp`, `server/core/storage/unit_of_work.hpp`, `server/core/storage/repositories.hpp`, `server/core/storage/db_worker_pool.hpp` | Repository and unit-of-work contracts for DB access. |
| Metrics/Lifecycle | `[Stable]+[Transitional]` | `server/core/metrics/http_server.hpp`, `server/core/metrics/build_info.hpp`, `server/core/runtime_metrics.hpp` | Operational metrics and lifecycle visibility. |
| Protocol | `[Stable]` | `server/core/protocol/packet.hpp`, `server/core/protocol/protocol_flags.hpp`, `server/core/protocol/protocol_errors.hpp`, `server/core/protocol/system_opcodes.hpp` | Wire header, flags, errors, and opcode constants. |
| Utilities | `[Stable]+[Transitional]+[Internal]` | `server/core/util/log.hpp`, `server/core/util/paths.hpp`, `server/core/util/service_registry.hpp` | Cross-binary helpers and process utilities. |

## Canonical Include Contract
- Use only headers classified as `[Stable]` for public consumers.
- Include form: `#include "server/core/..."` or `<server/core/...>`.
- Do not include implementation paths (`core/src/**`) or `Internal` headers.
- Internal header inventory is documented only in `docs/core-api-boundary.md`.

## Related Documents
- Boundary: `docs/core-api-boundary.md`
- Compatibility Policy: `docs/core-api/compatibility-policy.md`
- Quickstart: `docs/core-api/quickstart.md`
