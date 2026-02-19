# Networking API Guide

## Stability

| Header | Stability |
|---|---|
| `server/core/net/hive.hpp` | `[Stable]` |
| `server/core/net/dispatcher.hpp` | `[Stable]` |
| `server/core/net/listener.hpp` | `[Transitional]` |
| `server/core/net/connection.hpp` | `[Transitional]` |
| `server/core/net/acceptor.hpp` | `[Transitional]` |
| `server/core/net/session.hpp` | `[Transitional]` |

## Core Contracts
- `Hive` owns run/stop lifecycle for shared `io_context` usage.
- `Dispatcher` maps `msg_id` to handlers and does not own business logic.
- `Listener/Connection/Acceptor/Session` are transport/session primitives; treat them as transitional contracts until promoted.

## Ownership and Lifetime Rules
- Use `std::shared_ptr` ownership for async transport objects.
- Keep callback handlers non-blocking and exception-safe.
- Do not access internal mutable state directly across module boundaries.
