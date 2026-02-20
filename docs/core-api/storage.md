# Storage API Guide

## Stability
- This module currently has no `[Stable]` public headers.
- Storage contracts are classified as `[Internal]` and may change without compatibility guarantees.

## Current Contract Shape
- Internal contracts cover repository interfaces, `IUnitOfWork`, `IConnectionPool`, and `DbWorkerPool`.
- Repository DTO and interface sets remain chat-domain specific (`user/room/message/membership/session`).
- Public engine consumers should avoid direct dependency on storage internals.

## Usage Rules
- Keep repository calls inside `IUnitOfWork` commit/rollback boundaries.
- Keep async DB execution behind app/service adapters.
- Treat all storage symbols as internal until a generic, engine-neutral SPI is introduced.
