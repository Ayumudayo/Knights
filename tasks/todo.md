# Active TODO

> Status (2026-03-07): Active backlog only. Completed execution history was intentionally pruned.
> Historical detail: use git history plus archived task docs under `tasks/`.

## 1) Transport / RUDP

- [ ] mixed traffic(TCP+UDP) 장시간 soak 테스트를 실행하고 결과를 기록한다.
- [ ] RUDP 운영 검증을 마감한다.
  - OFF 불변성
  - ON canary fallback
  - 운영/롤백 리허설 근거
- [ ] NAT rebinding / MTU / 메모리 / 보안 강화 항목을 설계 문서 + 후속 이슈로 분리해 추적한다.

## 2) CI Cache / Toolchain Strategy

- [ ] `CI Prewarm`이 PR CI hit rate에 미치는 영향을 최소 1주간 측정한다.
- [ ] `Windows SCCache PoC`의 적용 전/후 compile 시간을 비교하고 채택/비채택을 결정한다.
- [ ] Conan2 binary remote 전략을 현재 캐시 방식과 비교하고 Go/No-Go 결론을 낸다.

## 3) CI / Repo Ops

- [ ] `main` branch protection과 required check 정책을 정리한다.
  - 기본 required: `CI`
  - path-gated workflow는 optional 유지 또는 별도 no-op wrapper 설계 후 required 승격

## Archived References

- `tasks.md`
  - Archived snapshot
- `tasks/next-plan.md`
  - Archived
- `tasks/core-engine-full-todo.md`
  - Archived
- `tasks/runtime-extensibility-todo.md`
  - Archived
- `tasks/cleanup-inspection-report-2026-03-05.md`
  - TODO hygiene review history
