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

## 3) TCP Load Generator

- [x] TCP load generator 요구사항/범위를 문서로 고정한다.
  - 대상: 기존 `haproxy -> gateway_app -> server_app`
  - 비대상: 별도 synthetic server 추가
- [x] `tools/loadgen` 스캐폴드를 추가한다.
  - headless CLI
  - 시나리오/리포트 출력 경로
- [x] 기존 프로토콜 자산을 재사용해 TCP 클라이언트 시뮬레이션을 구현한다.
  - login
  - join
  - chat / ping
- [x] 최소 시나리오 2개를 제공한다.
  - steady chat
  - mixed session soak
- [x] 결과 요약을 JSON 또는 동등한 기계 판독 형식으로 출력한다.
  - success/error count
  - latency summary
  - throughput
- [x] Docker stack 기준 실행/검증 절차를 문서화한다.

### Review

- `pwsh scripts/build.ps1 -Config Release -Target tcp_loadgen`
  - `tcp_loadgen.exe` Release 빌드 성공
- `pwsh scripts/deploy_docker.ps1 -Action up -Detached -Build`
  - Docker stack + HAProxy frontend 기동 성공
- `python tests/python/verify_chat.py`
  - PASS
- `python tests/python/verify_pong.py`
  - PASS
- `build-windows\\Release\\tcp_loadgen.exe --host 127.0.0.1 --port 6000 --scenario tools/loadgen/scenarios/steady_chat.json --report build/loadgen/steady_chat.json`
  - `connected=24 authenticated=24 joined=24 success=155 errors=0 throughput_rps=8.60 p95_ms=14.11`
- `build-windows\\Release\\tcp_loadgen.exe --host 127.0.0.1 --port 6000 --scenario tools/loadgen/scenarios/mixed_session_soak.json --report build/loadgen/mixed_session_soak.json`
  - `connected=24 authenticated=24 joined=12 success=84 errors=0 throughput_rps=4.60 p95_ms=14.51`
- 참고
  - fresh stack 기준 loadgen 구현/검증 완료
  - repeated same-stack run 회귀는 아래 Gateway Follow-up에서 별도로 마감

## 4) Gateway Follow-up

- [x] repeated loadgen run에서 드러난 `gateway_app` 종료 경로 결함을 수정한다.
  - 재현: back-to-back loadgen run
  - 증상: `free(): double free detected in tcache 2`
  - 범위: `GatewayApp::BackendConnection` close/write/read 수명 관리

### Review

- `pwsh scripts/build.ps1 -Config Release -Target gateway_app`
  - gateway 수정 반영 후 Release 빌드 성공
- `pwsh scripts/deploy_docker.ps1 -Action up -Detached -Build`
  - 수정 반영 Docker image 재빌드 성공
- same-stack repeated run
  - `python tests/python/verify_chat.py` PASS
  - `steady_chat` 1회차: `connected=24 authenticated=24 joined=24 success=152 errors=0 throughput_rps=8.44 p95_ms=16.13`
  - `mixed_session_soak` 1회차: `connected=24 authenticated=24 joined=12 success=81 errors=0 throughput_rps=4.44 p95_ms=15.67`
  - `steady_chat` 2회차: `connected=24 authenticated=24 joined=24 success=153 errors=0 throughput_rps=8.50 p95_ms=14.37`
  - `mixed_session_soak` 2회차: `connected=24 authenticated=24 joined=12 success=84 errors=0 throughput_rps=4.60 p95_ms=14.06`
- `python tests/python/verify_pong.py`
  - repeated run 이후에도 PASS
- container health
  - `gateway-1`, `gateway-2` 컨테이너가 same-stack repeated run 이후에도 `Up` 유지
  - gateway logs에 `free(): double free detected in tcache 2` 재발 없음

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
