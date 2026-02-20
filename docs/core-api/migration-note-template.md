# Core API Migration Note Template

## Summary
- Change:
- Impacted API:
- Stability Level:

## Why
- Problem being solved:
- Reason previous API was insufficient:

## Breaking Surface
- Removed/renamed symbols:
- Signature changes:
- Behavior changes:

## Migration Steps
1. Update includes:
2. Update call sites:
3. Validate behavior:

## Before
```cpp
// old usage
```

## After
```cpp
// new usage
```

## Validation
- Build/test evidence:
- Runtime/metrics verification:

## Deprecation Notice Template
Use when a `Stable` API is deprecated but not yet removed.

```md
### Deprecation Notice
- Deprecated API: `<symbol or header>`
- Replacement: `<symbol or header>`
- First deprecated in: `<version>`
- Planned removal: `<version or release window>`
- Migration note: `docs/core-api/<file>.md`
```

## Release Notes Entry Format
Use this format in `docs/core-api/changelog.md`.

```md
### Changed
- `<short behavior/API summary>`

### Breaking
- `<breaking change summary>`
- Migration: `docs/core-api/<migration-note>.md`

### Deprecated
- `<deprecated API and replacement>`
```
