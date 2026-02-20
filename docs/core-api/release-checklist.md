# Core API Release Checklist

## Scope
- Applies when shipping changes that touch `Stable` headers in `core/include/server/core/`.

## Required Inputs
- Target release version (`major.minor.patch`).
- Change set (PR list or commit range).
- Any required migration notes under `docs/core-api/`.

## Release Gates
- [ ] `core/include/server/core/api/version.hpp` updated for this release.
- [ ] `docs/core-api/compatibility-matrix.json` `api_version` matches `version.hpp`.
- [ ] Breaking `Stable` API changes have migration notes using `docs/core-api/migration-note-template.md`.
- [ ] `docs/core-api/changelog.md` has an entry for this release.
- [ ] Public consumer targets build:
  - `core_public_api_smoke`
  - `core_public_api_headers_compile`
  - `core_public_api_stable_header_scenarios`
- [ ] API governance checks pass:
  - `python tools/check_core_api_contracts.py --check-boundary`
  - `python tools/check_core_api_contracts.py --check-boundary-fixtures`
  - `python tools/check_core_api_contracts.py --check-stable-governance-fixtures`

## Verification Commands

```powershell
python tools/check_core_api_contracts.py --check-boundary
python tools/check_core_api_contracts.py --check-boundary-fixtures
python tools/check_core_api_contracts.py --check-stable-governance-fixtures

pwsh scripts/build.ps1 -Config Debug -Target core_public_api_smoke
pwsh scripts/build.ps1 -Config Debug -Target core_public_api_headers_compile
pwsh scripts/build.ps1 -Config Debug -Target core_public_api_stable_header_scenarios

ctest --preset windows-test -R "CorePublicApi|CoreApiBoundaryFixtures|CoreApiStableGovernanceFixtures" --output-on-failure
```

## Sign-off Record
- Release version:
- Reviewer:
- Date (UTC):
- Notes:
