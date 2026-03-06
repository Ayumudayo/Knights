# Active TODO

> Status (2026-03-07): Active backlog only. Completed execution history was intentionally pruned.
> Historical detail: use git history plus archived task docs under `tasks/`.

## 1) Transport / RUDP

- [ ] RUDP 운영 검증을 마감한다.
  - OFF 불변성
  - ON canary fallback
  - 운영/롤백 리허설 근거
- [ ] NAT rebinding / MTU / 메모리 / 보안 강화 항목을 설계 문서 + 후속 이슈로 분리해 추적한다.

## 2) CI / Repo Ops

- [ ] `main` branch protection과 required check 정책을 정리한다.
  - 기본 required: `CI`
  - path-gated workflow는 optional 유지 또는 별도 no-op wrapper 설계 후 required 승격

## Quantitative Validation

- 정량적 검증 backlog는 `tasks/quantitative-validation.md`에서 별도 추적한다.

## Archived References

- `tasks.md`
  - Archived snapshot
- `tasks/next-plan.md`
  - Archived
- `tasks/core-engine-full-todo.md`
  - Archived
- `tasks/quantitative-validation.md`
  - Active measurement / benchmark backlog
