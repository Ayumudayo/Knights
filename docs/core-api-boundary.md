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
| `server/core/app/termination_signals.hpp` | Stable | Process-global termination polling contract for non-Asio loops and shared shutdown signaling. |
| `server/core/build_info.hpp` | Stable | Build metadata contract used by all binaries. |
| `server/core/compression/compressor.hpp` | Stable | LZ4 compress/decompress contract with explicit error signaling on malformed or invalid input. |
| `server/core/concurrent/job_queue.hpp` | Stable | Bounded/unbounded FIFO queue contract with explicit stop and backpressure behavior. |
| `server/core/concurrent/locked_queue.hpp` | Internal | Low-level queue primitive intended for internal worker plumbing. |
| `server/core/concurrent/task_scheduler.hpp` | Stable | Clear scheduling contract and cross-module usage. |
| `server/core/concurrent/thread_manager.hpp` | Stable | Fixed worker-pool consumer for `JobQueue` with idempotent stop and guarded start semantics. |
| `server/core/config/options.hpp` | Stable | Session runtime options consumed by networking path. |
| `server/core/memory/memory_pool.hpp` | Stable | Fixed-block allocator + RAII buffer contract with bounded failure (`Acquire()==nullptr`) semantics. |
| `server/core/metrics/build_info.hpp` | Stable | Shared Prometheus build-info helper. |
| `server/core/metrics/http_server.hpp` | Stable | Shared admin/metrics HTTP surface. |
| `server/core/metrics/metrics.hpp` | Stable | Named metric accessor contract with no-op fallback for backend-optional operation. |
| `server/core/net/acceptor.hpp` | Internal | Server-specific accept loop coupled to `SessionOptions`/`ConnectionRuntimeState`; not part of stable transport contract. |
| `server/core/net/connection.hpp` | Stable | Extensible transport base with FIFO send-queue ordering, bounded queue backpressure, and idempotent stop lifecycle. |
| `server/core/net/dispatcher.hpp` | Stable | Core msg_id routing contract. |
| `server/core/net/hive.hpp` | Stable | `io_context` lifecycle wrapper shared by transport modules. |
| `server/core/net/listener.hpp` | Stable | Generic accept loop contract with injected connection factory and idempotent stop semantics. |
| `server/core/net/connection_runtime_state.hpp` | Internal | Internal session-runtime state contract for connection-count guardrail and randomized session-id seed. |
| `server/core/net/session.hpp` | Internal | Server packet/session implementation tied to dispatcher/options/shared runtime state. |
| `server/core/protocol/packet.hpp` | Stable | Wire header encode/decode contract. |
| `server/core/protocol/protocol_errors.hpp` | Stable | Shared error code constants for protocol responses. |
| `server/core/protocol/protocol_flags.hpp` | Stable | Shared protocol flags/capability constants. |
| `server/core/protocol/system_opcodes.hpp` | Stable | Generated opcode contract consumed by server/client paths. |
| `server/core/runtime_metrics.hpp` | Stable | Process-wide runtime counters/snapshot contract used by server/gateway/tools observability paths. |
| `server/core/security/cipher.hpp` | Stable | AES-256-GCM encrypt/decrypt contract with key/IV size validation and authentication failure signaling. |
| `server/core/storage/connection_pool.hpp` | Internal | Server storage adapter contract (`IConnectionPool`) remains domain-coupled through chat repositories/UoW. |
| `server/core/storage/db_worker_pool.hpp` | Internal | Async DB execution helper is server-internal plumbing over internal storage contracts. |
| `server/core/storage/repositories.hpp` | Internal | Repository DTO/interfaces are chat-domain specific and excluded from stable engine API. |
| `server/core/storage/unit_of_work.hpp` | Internal | Transaction boundary depends on internal, chat-coupled repository interfaces. |
| `server/core/util/crash_handler.hpp` | Internal | Process-level crash hook intended for app entrypoints. |
| `server/core/util/log.hpp` | Stable | Common logging contract used by all binaries. |
| `server/core/util/paths.hpp` | Stable | Executable path helper used by tools and services. |
| `server/core/util/service_registry.hpp` | Stable | Typed service registration/lookup contract used by multi-binary runtime composition. |

## Public API Naming Rules (Phase 1)
- Public symbols should live under `server::core::<module>` and avoid global aliases.
- Public API should avoid mutable public data fields; use methods that define behavior.
- Public headers must not require `server/` or `gateway/` implementation headers.
- Public docs/examples must not include `Internal` headers.

## Immediate Follow-up
- Keep `Transitional` header count at zero; new exposed headers must be classified directly as `Stable` or `Internal`.
- Add CI guard so public examples compile with `Stable` headers only.
- Keep `docs/core-api/compatibility-matrix.json` synchronized with `Stable` header inventory and API version.
