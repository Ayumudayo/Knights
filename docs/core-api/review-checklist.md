# Core API PR Review Checklist

- [ ] API scope matches `docs/core-api-boundary.md` classification.
- [ ] Ownership/lifetime contract is explicit and consistent.
- [ ] Thread-safety and error behavior are documented.
- [ ] Compatibility impact is classified (`Stable`/`Transitional`/`Internal`).
- [ ] Migration note added for breaking changes.
- [ ] Domain docs under `docs/core-api/` updated in same PR.
- [ ] Public API smoke consumer builds successfully.
- [ ] Extension ABI impact is classified and compatibility strategy is documented.
- [ ] Extension ABI deprecation/removal includes explicit migration guidance.
- [ ] Gateway extension contract changes align with `docs/core-api/gateway-extension-interface.md`.
- [ ] Write-behind extension contract changes align with `docs/core-api/write-behind-extension-interface.md`.
