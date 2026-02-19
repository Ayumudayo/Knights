# Write-Behind Extension Interface Candidates

## Purpose
- Define design targets for write-behind (`wb_worker`) extension points around event processing.
- Preserve reliability and observability guarantees while allowing future transform/filter logic.

## Candidate Hook Surface
- **Pre-write transform hook**: receives decoded stream event and returns transformed event payload.
- **Filter hook**: returns keep/drop decision with reason code before DB write.
- **Failure classification hook**: maps write/validation failures to retry, DLQ, or drop policy.

## Failure Semantics
- Hook exceptions/failures default to conservative path: no silent success.
- If classification hook fails, event follows existing error handling policy (`WB_ACK_ON_ERROR`, DLQ/dead stream settings).
- Any drop decision must emit a reason-tagged counter for auditability.

## Observability Contract
- Required metrics for future extension path:
  - hook invocation total
  - hook error total
  - transform/filter decision totals
  - fallback path total
- Metrics must remain Prometheus-friendly and consistent with current `wb_*` naming style.

## Compatibility Surface
- Stable candidate interface should define fixed event DTO version and hook result schema.
- Breaking hook contract changes require migration notes and compatibility classification updates.

## Non-Goals (This Phase)
- No runtime hook execution engine implementation.
- No change to existing write-behind persistence flow.
