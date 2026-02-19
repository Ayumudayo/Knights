# Core Extension Contracts

## Purpose
- Document extension surfaces that affect long-term API governance.
- Classify maturity and compatibility expectations for each extension contract.

## Current Extension Surfaces

| Surface | Location | Maturity | Notes |
|---|---|---|---|
| Chat hook plugin ABI | `server/include/server/chat/chat_hook_plugin_abi.hpp` | Transitional | Explicit plugin entry contract exists; hot-reload manager/chain are implemented in `server/src/chat/chat_hook_plugin_manager.*` and `server/src/chat/chat_hook_plugin_chain.*`. |

## Governance Rules for Extension ABI
- Extension ABI changes must be classified as compatible or breaking in PR description.
- Breaking ABI changes require migration notes under `docs/core-api/` before merge.
- Extension ABI docs must be updated in the same PR as ABI shape changes.
- Plugin loader behavior changes must preserve operational safety (lock/sentinel and reload semantics) or include explicit migration guidance.

## Candidate Next Contracts (Design Target)
- Gateway extension interface design: `docs/core-api/gateway-extension-interface.md`.
- Write-behind extension interface design: `docs/core-api/write-behind-extension-interface.md`.

## Non-Goals for This Phase
- No new runtime extension mechanism implementation.
- No protocol-level redesign.
