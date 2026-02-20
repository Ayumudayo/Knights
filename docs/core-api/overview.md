# server_core API Overview

## Status Legend
- `[Stable]`: Public contract with compatibility promise.
- `[Transitional]`: Public today, still being hardened.
- `[Internal]`: Not for public/sample usage.

## Module Map

| Module | Stability | Primary Headers | Purpose |
|---|---|---|---|
| Runtime Host | `[Stable]` | `server/core/app/app_host.hpp`, `server/core/app/termination_signals.hpp` | Process lifecycle, readiness/health, and process-level termination signaling. |
| Networking | `[Stable]+[Internal]` | `server/core/net/hive.hpp`, `server/core/net/dispatcher.hpp`, `server/core/net/listener.hpp`, `server/core/net/connection.hpp` | Event loop lifecycle + transport routing primitives. |
| Concurrency | `[Stable]+[Internal]` | `server/core/concurrent/task_scheduler.hpp`, `server/core/concurrent/job_queue.hpp`, `server/core/concurrent/thread_manager.hpp` | Scheduler and worker queue primitives. |
| Compression | `[Stable]` | `server/core/compression/compressor.hpp` | LZ4-based byte payload compression/decompression contract. |
| Memory | `[Stable]` | `server/core/memory/memory_pool.hpp` | Fixed-size memory pool and RAII buffer manager contract. |
| Storage SPI | `[Internal]` | (No Stable public headers) | Server-specific repository/UoW and async DB worker primitives. |
| Metrics/Lifecycle | `[Stable]` | `server/core/metrics/metrics.hpp`, `server/core/metrics/http_server.hpp`, `server/core/metrics/build_info.hpp`, `server/core/runtime_metrics.hpp` | Operational metrics and lifecycle visibility. |
| Protocol | `[Stable]` | `server/core/protocol/packet.hpp`, `server/core/protocol/protocol_flags.hpp`, `server/core/protocol/protocol_errors.hpp`, `server/core/protocol/system_opcodes.hpp` | Wire header, flags, errors, and opcode constants. |
| Security | `[Stable]` | `server/core/security/cipher.hpp` | AES-256-GCM encryption/decryption contract with authenticated payload semantics. |
| Utilities | `[Stable]+[Internal]` | `server/core/util/log.hpp`, `server/core/util/paths.hpp`, `server/core/util/service_registry.hpp` | Cross-binary helpers and process utilities. |

## Canonical Include Contract
- Use only headers classified as `[Stable]` for public consumers.
- Include form: `#include "server/core/..."` or `<server/core/...>`.
- Do not include implementation paths (`core/src/**`) or `Internal` headers.
- Internal header inventory is documented only in `docs/core-api-boundary.md`.

## Related Documents
- Boundary: `docs/core-api-boundary.md`
- Compatibility Policy: `docs/core-api/compatibility-policy.md`
- Quickstart: `docs/core-api/quickstart.md`
