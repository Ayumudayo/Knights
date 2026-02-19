# Storage API Guide

## Stability

| Header | Stability |
|---|---|
| `server/core/storage/connection_pool.hpp` | `[Transitional]` |
| `server/core/storage/unit_of_work.hpp` | `[Transitional]` |
| `server/core/storage/repositories.hpp` | `[Transitional]` |
| `server/core/storage/db_worker_pool.hpp` | `[Transitional]` |

## Current Contract Shape
- Storage contracts are exposed for reuse, but still chat-domain coupled.
- `IUnitOfWork` defines transaction boundary and repository access.
- `DbWorkerPool` provides async execution wrapper over `IConnectionPool`.

## Usage Rules
- Keep repository calls inside `IUnitOfWork` boundaries.
- Treat DTO/repository fields as unstable until promoted to `[Stable]`.
- Prefer adapter layers in app modules to isolate future storage contract changes.
