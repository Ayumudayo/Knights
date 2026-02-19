# Gateway Extension Interface Candidates

## Purpose
- Define a design target for gateway-side extension points without adding runtime extension code in this phase.
- Keep compatibility boundaries explicit before any gateway hook surface is promoted.

## Candidate Hook Surface
- **Routing policy hook**: receives frontend session metadata and backend candidate set; returns selected backend id.
- **Admission policy hook**: receives connection metadata and returns allow/deny with reason code.
- **Session lifecycle hook**: receives connect/disconnect/reconnect events for observability/policy side effects.

## Lifecycle Contract
- Hook initialization runs during gateway bootstrap before listener accept loop starts.
- Hook callbacks execute on gateway request path and must be non-blocking.
- Hook shutdown runs during graceful stop before backend connection teardown completes.

## Error Model
- Hook load/initialize failure: gateway startup fails fast with explicit error log.
- Hook callback failure: gateway falls back to built-in default policy and increments failure metric.
- Hook timeout: treated as callback failure; default policy path is used.

## Compatibility Surface
- Stable candidate interface should define:
  - immutable input DTO shape
  - deterministic callback return schema
  - explicit timeout/deadline behavior
- Breaking changes require migration notes and compatibility classification updates.

## Non-Goals (This Phase)
- No new gateway plugin loader implementation.
- No dynamic code loading policy decisions.
