# Concurrency API Guide

## Stability

| Header | Stability |
|---|---|
| `server/core/concurrent/task_scheduler.hpp` | `[Stable]` |
| `server/core/concurrent/job_queue.hpp` | `[Transitional]` |
| `server/core/concurrent/thread_manager.hpp` | `[Transitional]` |

## Core Contracts
- `TaskScheduler` is pull-based: caller owns poll loop and execution cadence.
- Transitional worker queue APIs remain available but can change during hardening.
- Internal queue primitives are reserved for runtime plumbing and are excluded from public examples.

## Usage Rules
- Keep scheduled tasks short and side-effect scoped.
- Prefer explicit shutdown sequencing for worker resources.
- Avoid coupling business logic to queue internals.
