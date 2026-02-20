# Concurrency API Guide

## Stability

| Header | Stability |
|---|---|
| `server/core/concurrent/task_scheduler.hpp` | `[Stable]` |
| `server/core/concurrent/job_queue.hpp` | `[Stable]` |
| `server/core/concurrent/thread_manager.hpp` | `[Stable]` |

## Core Contracts
- `TaskScheduler` is pull-based: caller owns poll loop and execution cadence.
- `JobQueue` provides thread-safe FIFO semantics with explicit `Stop()`-driven shutdown signaling.
- `ThreadManager` consumes `JobQueue` with guarded `Start()` and idempotent `Stop()` semantics.
- Internal queue primitives are reserved for runtime plumbing and are excluded from public examples.

## Usage Rules
- Keep scheduled tasks short and side-effect scoped.
- Prefer explicit shutdown sequencing for worker resources.
- Avoid coupling business logic to queue internals.
