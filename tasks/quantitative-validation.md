# Quantitative Validation Backlog

> Status (2026-03-07): Active measurement / benchmark backlog.
> Scope: 숫자 기반 판단이 필요한 soak, hit-rate, compile-time, toolchain 비교 항목만 추적한다.

## 1) Transport / RUDP

- [ ] mixed traffic(TCP+UDP) 장시간 soak 테스트를 실행하고 결과를 기록한다.
  - 기준:
    - duration
    - error rate
    - reconnect / fallback count
    - 핵심 latency / throughput 요약

## 2) CI Cache / Build Throughput

- [ ] `CI Prewarm`이 PR CI hit rate에 미치는 영향을 최소 1주간 측정한다.
  - 기준:
    - exact hit rate
    - restore elapsed time
    - main / PR 간 drift

- [ ] `Windows SCCache PoC`의 적용 전/후 compile 시간을 비교하고 채택/비채택을 결정한다.
  - 기준:
    - cold build time
    - warm build time
    - cache hit ratio
    - 운영 복잡도

## 3) Toolchain Strategy

- [ ] Conan2 binary remote 전략을 현재 캐시 방식과 비교하고 Go/No-Go 결론을 낸다.
  - 기준:
    - end-to-end CI wall clock
    - cache miss recovery time
    - 운영 복잡도
    - 재현성 / 장애 복구성

## Notes

- 실행 backlog / 운영 마감 항목은 `tasks/todo.md`에서 관리한다.
- 완료 이력과 장문 배경은 git history 및 archived task docs를 참고한다.
