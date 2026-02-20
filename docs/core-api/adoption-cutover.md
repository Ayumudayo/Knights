# Core API Adoption and Cutover Plan

## Goal
- Move `server`, `gateway`, and `tools` toward `Stable`-only consumption for `core` public contracts.
- Keep `Transitional` header count at zero.

## Current Audit Snapshot

### Transitional headers
- `docs/core-api-boundary.md` currently has zero `Transitional` rows.

### Internal header usage by top-level modules
- `server` currently includes internal headers in a small set of implementation paths:
  - `server/src/app/core_internal_adapter.cpp` -> internal accept-loop/network/runtime-state/storage worker/crash-hook contracts (adapter boundary)
  - `server/src/storage/postgres/connection_pool.cpp` -> internal storage repository/transaction contracts
  - `server/src/chat/chat_service_core.cpp` -> internal storage connection-pool contract
  - `server/src/chat/handlers_login.cpp` -> internal storage connection-pool contract
  - `server/src/chat/handlers_join.cpp` -> internal storage connection-pool contract
  - `server/src/chat/handlers_chat.cpp` -> internal storage connection-pool contract
  - `server/src/chat/handlers_leave.cpp` -> internal storage connection-pool contract
- `gateway` internal-header include hits: none (current grep audit).
- `tools` internal-header include hits: none (current grep audit).

## Cutover Phases

### Phase A - Server network boundary adapter
- Introduce server-local adapters around internal session/runtime-state usage.
- Limit direct internal includes to adapter translation units.
- Keep public-facing server module boundaries on `Stable` contracts.
- Status: started.
  - Added `server/include/server/app/core_internal_adapter.hpp` and `server/src/app/core_internal_adapter.cpp`.
  - `server/src/app/bootstrap.cpp` now consumes adapter APIs for crash-handler installation, runtime connection-count access, session-listener start/stop handling, and DB pool/worker lifecycle.
  - `server/src/app/router.cpp` removed direct session-header include and uses `ChatService::NetSession` alias.
  - `server/include/server/storage/postgres/connection_pool.hpp` now forward-declares storage contracts instead of directly including internal storage header.

### Phase B - Server storage boundary adapter
- Move chat-domain storage coupling behind server-local interfaces.
- Keep `core` storage headers internal; avoid exposing those includes beyond server storage implementation.
- Status: in progress.
  - Removed direct `repositories.hpp` / `unit_of_work.hpp` includes from chat handlers and `chat_service_core`; storage API use now funnels through `connection_pool.hpp` surface.

### Phase C - Enforcement and regression
- Enforce `Stable` include-only policy for public examples and consumer tests.
- Keep governance checks in CI for boundary and stable-governance fixtures.

## Verification Record
- Boundary contract check: `python tools/check_core_api_contracts.py --check-boundary`
- Boundary fixture check: `python tools/check_core_api_contracts.py --check-boundary-fixtures`
- Stable governance fixture check: `python tools/check_core_api_contracts.py --check-stable-governance-fixtures`
- Consumer tests: `ctest --preset windows-test -R "CorePublicApi|CoreApiBoundaryFixtures|CoreApiStableGovernanceFixtures" --output-on-failure`

## Exit Criteria
- `docs/core-api-boundary.md` remains at `Transitional = 0`.
- `gateway` and `tools` remain free of internal `core` header includes.
- `server` internal includes are contained to implementation adapters and not propagated into public/example contracts.
