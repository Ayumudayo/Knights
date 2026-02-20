# Networking API Guide

## Stability

| Header | Stability |
|---|---|
| `server/core/net/hive.hpp` | `[Stable]` |
| `server/core/net/dispatcher.hpp` | `[Stable]` |
| `server/core/net/listener.hpp` | `[Stable]` |
| `server/core/net/connection.hpp` | `[Stable]` |

## Core Contracts
- `Hive` owns run/stop lifecycle for shared `io_context` usage.
- `Dispatcher` maps `msg_id` to handlers and does not own business logic.
- `Listener` owns the accept loop lifecycle and injects transport creation through `connection_factory`.
- `Connection` owns async read/write loops with FIFO write ordering and bounded send-queue backpressure.
- Server-specific packet session implementation (`acceptor`/`session`) is internal and excluded from public API usage.

## Ownership and Lifetime Rules
- Use `std::shared_ptr` ownership for async transport objects.
- Keep callback handlers non-blocking and exception-safe.
- Do not access internal mutable state directly across module boundaries.
