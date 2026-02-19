# server_core Public API Boundary (Phase 1)

## Purpose
- Define which headers are treated as real API contracts.
- Separate `Stable Public API` from `Transitional` and `Internal` headers.
- Prevent accidental dependency on unstable internals.

## Compatibility Levels
- `Stable`: Compatibility promise applies. Breaking changes require migration notes.
- `Transitional`: Exposed today but may change while API hardening continues.
- `Internal`: No compatibility promise. Not allowed in public examples.

## Canonical Include Contract
- Public consumers MUST include only headers listed as `Stable` in this document.
- Public include form is `#include "server/core/..."` (or `<server/core/...>`), never implementation paths.
- `core/include/server/core/protocol/system_opcodes.hpp` is generated; modify JSON + generator, not the header.
- Headers marked `Internal` are forbidden for sample/public documentation usage.

## Header Inventory and Classification

| Header | Level | Notes |
|---|---|---|
| `server/core/api/version.hpp` | Stable | Public API version signal; stable-header changes require this version to be updated. |
| `server/core/app/app_host.hpp` | Stable | Runtime host contract used across server/gateway/tools. |
| `server/core/app/termination_signals.hpp` | Transitional | Process-global signal flag API; may move behind `AppHost` only. |
| `server/core/build_info.hpp` | Stable | Build metadata contract used by all binaries. |
| `server/core/compression/compressor.hpp` | Transitional | Algorithm and error contract may evolve. |
| `server/core/concurrent/job_queue.hpp` | Transitional | Legacy naming/namespace style; queue semantics need formal contract text. |
| `server/core/concurrent/locked_queue.hpp` | Internal | Low-level queue primitive intended for internal worker plumbing. |
| `server/core/concurrent/task_scheduler.hpp` | Stable | Clear scheduling contract and cross-module usage. |
| `server/core/concurrent/thread_manager.hpp` | Transitional | Basic worker wrapper; lifecycle API may be consolidated later. |
| `server/core/config/options.hpp` | Stable | Session runtime options consumed by networking path. |
| `server/core/config/runtime_settings.hpp` | Internal | Chat-domain setting key registry, not general core contract. |
| `server/core/memory/memory_pool.hpp` | Transitional | Raw-pointer/legacy method style; API cleanup pending. |
| `server/core/metrics/build_info.hpp` | Stable | Shared Prometheus build-info helper. |
| `server/core/metrics/http_server.hpp` | Stable | Shared admin/metrics HTTP surface. |
| `server/core/metrics/metrics.hpp` | Transitional | Generic metrics abstraction exists but backend contract is minimal. |
| `server/core/net/acceptor.hpp` | Transitional | Tightly coupled to `Session`; naming alignment still in progress. |
| `server/core/net/connection.hpp` | Transitional | Extensible transport base, but callback contract needs hardening. |
| `server/core/net/dispatcher.hpp` | Stable | Core msg_id routing contract. |
| `server/core/net/hive.hpp` | Stable | `io_context` lifecycle wrapper shared by transport modules. |
| `server/core/net/listener.hpp` | Transitional | Generic listener API still evolving with connection abstractions. |
| `server/core/net/session.hpp` | Transitional | Exposes mutable members; boundary tightening required. |
| `server/core/protocol/packet.hpp` | Stable | Wire header encode/decode contract. |
| `server/core/protocol/protocol_errors.hpp` | Stable | Shared error code constants for protocol responses. |
| `server/core/protocol/protocol_flags.hpp` | Stable | Shared protocol flags/capability constants. |
| `server/core/protocol/system_opcodes.hpp` | Stable | Generated opcode contract consumed by server/client paths. |
| `server/core/runtime_metrics.hpp` | Transitional | Process-global counters; naming/ownership policy still being finalized. |
| `server/core/security/cipher.hpp` | Transitional | Crypto API is usable but still implementation-shaped. |
| `server/core/state/shared_state.hpp` | Internal | Session runtime state holder for server internals. |
| `server/core/storage/connection_pool.hpp` | Transitional | Storage SPI is useful but repository contract set is still stabilizing. |
| `server/core/storage/db_worker_pool.hpp` | Transitional | Internal async DB execution helper exposed today for reuse. |
| `server/core/storage/repositories.hpp` | Transitional | DTO/repository interfaces are chat-domain coupled today. |
| `server/core/storage/unit_of_work.hpp` | Transitional | Depends on repository set that is still being normalized. |
| `server/core/util/crash_handler.hpp` | Internal | Process-level crash hook intended for app entrypoints. |
| `server/core/util/log.hpp` | Stable | Common logging contract used by all binaries. |
| `server/core/util/paths.hpp` | Stable | Executable path helper used by tools and services. |
| `server/core/util/service_registry.hpp` | Transitional | Service locator contract exists but intended for constrained internal use. |

## Public API Naming Rules (Phase 1)
- Public symbols should live under `server::core::<module>` and avoid global aliases.
- Public API should avoid mutable public data fields; use methods that define behavior.
- Public headers must not require `server/` or `gateway/` implementation headers.
- Public docs/examples must not include `Internal` headers.

## Immediate Follow-up
- Convert Transitional headers to either `Stable` (after contract hardening) or `Internal` (if app-internal).
- Add CI guard so public examples compile with `Stable` headers only.
- Keep `docs/core-api/compatibility-matrix.json` synchronized with `Stable` header inventory and API version.
