# server_core Compatibility Policy

## Scope
- Applies to headers marked `Stable` in `docs/core-api-boundary.md`.
- Does not apply to `Transitional` or `Internal` headers.

## Stability Badges
- `[Stable]`: Compatibility promise applies.
- `[Transitional]`: Subject to change while hardening.
- `[Internal]`: No compatibility promise.

## Breaking Change Rules (`[Stable]`)
- Breaking:
  - Removing or renaming public types/functions/constants.
  - Changing function signatures or callback contracts.
  - Changing ownership/lifetime semantics in incompatible ways.
  - Tightening behavior that can reject previously valid inputs.
- Non-breaking:
  - Adding new overloads or optional fields with safe defaults.
  - Adding new constants without changing existing values.
  - Documentation-only clarifications.

## Deprecation Policy
- Mark deprecated APIs in docs before removal.
- Keep deprecated API for at least one release cycle.
- Removal requires migration notes using `docs/core-api/migration-note-template.md`.

## PR Requirements for Stable API Changes
- Update domain docs under `docs/core-api/`.
- Add or update migration notes for breaking changes.
- Pass API smoke consumer build and CI checks.
- Update `core/include/server/core/api/version.hpp`.
- Update `docs/core-api/compatibility-matrix.json`.
