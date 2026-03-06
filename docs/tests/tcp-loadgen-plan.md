# TCP Load Generator Plan

상태: implemented and locally verified

브랜치: `feature/tcp-loadgen`

## 1. 목적

기존 서비스 경로

- `haproxy -> gateway_app -> server_app`

를 그대로 대상으로 두고, 다수의 가짜 TCP 클라이언트를 붙여 정량 검증을 수행하는 headless load generator를 추가한다.

이 도구의 1차 목표는 다음과 같다.

- mixed session soak의 재현 가능한 실행 경로 확보
- steady chat / ping / login / join 부하의 latency / throughput 측정
- `tests/python/verify_soak_perf_gate.py`보다 높은 동시성, 더 긴 지속 시간, 더 명확한 결과 포맷 제공

## 2. 비목표

- 별도 synthetic server 구현
- UDP/RUDP 부하 경로 동시 구현
- GUI 포함 클라이언트
- 운영형 distributed coordinator/worker 구조

초기 버전은 단일 프로세스, TCP-only, headless CLI로 제한한다.

## 3. 재사용 자산

### 3.1 프로토콜/프레이밍

- [packet.hpp](../../core/include/server/core/protocol/packet.hpp)
  - `PacketHeader`
  - `encode_header()`
  - `decode_header()`
  - `write_lp_utf8()`
- [system_opcodes.hpp](../../core/include/server/core/protocol/system_opcodes.hpp)
- [game_opcodes.hpp](../../server/include/server/protocol/game_opcodes.hpp)
- [codec.hpp](../../core/include/server/wire/codec.hpp)

### 3.2 기존 클라이언트 구현 참고

- [net_client.cpp](../../client_gui/src/net_client.cpp)
  - TCP 송수신 루프
  - 로그인/입장/채팅 요청 직렬화
  - `MSG_HELLO`, `MSG_ERR`, `MSG_LOGIN_RES`, `MSG_CHAT_BROADCAST` 처리
- [verify_chat.py](../../tests/python/verify_chat.py)
  - 최소 TCP smoke 시나리오
- [verify_soak_perf_gate.py](../../tests/python/verify_soak_perf_gate.py)
  - 현재 soak/perf gate 기준과 출력 형태

## 4. 배치 위치

- 새 도구 경로: `tools/loadgen/`
- 새 실행 파일 이름: `tcp_loadgen`
- 새 문서/시나리오 경로:
  - `tools/loadgen/README.md`
  - `tools/loadgen/scenarios/*.json`

## 5. 빌드 정책

권장 정책:

- 루트 CMake에 `BUILD_LOADGEN_TOOLS` 옵션 추가
  - 기본값: `ON`
  - `BUILD_SERVER_STACK=ON`일 때만 허용
- 타깃은 `server_core`, `wire_proto`를 재사용한다.
- 새 외부 의존성은 추가하지 않는다.

이유:

- 기존 wire/protocol 자산을 그대로 재사용할 수 있다.
- 운영 바이너리와 독립적으로 빌드/배포할 수 있다.

## 6. 구조

초기 구조:

1. `session_client`
   - 단일 TCP 세션
   - connect / login / join / chat / ping
   - `gateway_app` 경로 기준 immediate `LOGIN_REQ`

2. `scenario_runner`
   - 세션 수, ramp-up, duration, message rate 제어
   - run별 unique room suffix 적용 가능

3. `metrics_collector`
   - latency samples
   - success/error count
   - reconnect / timeout count

4. `report_writer`
   - JSON summary 출력

## 7. 최소 시나리오

### 7.1 steady_chat

- N개 세션 로그인
- 동일 room 입장
- 일정 rate로 `MSG_CHAT_SEND`
- self-broadcast 또는 수신 확인

### 7.2 mixed_session_soak

- 일부 세션은 login only
- 일부 세션은 join + chat
- 일부 세션은 ping only
- duration 동안 steady-state 유지

## 8. CLI 초안

예시:

```text
tcp_loadgen --host 127.0.0.1 --port 6000 --scenario tools/loadgen/scenarios/steady_chat.json --report build/loadgen/steady_chat.json
```

필수 인자:

- `--host`
- `--port`
- `--scenario`
- `--report`

선택 인자:

- `--seed`
- `--verbose`

## 9. 시나리오 파일 초안

```json
{
  "name": "steady_chat",
  "sessions": 24,
  "ramp_up_ms": 3000,
  "duration_ms": 15000,
  "room": "loadgen_room",
  "room_password": "loadgen-shared",
  "unique_room_per_run": true,
  "message_bytes": 96,
  "login_prefix": "steady_chat",
  "groups": [
    {
      "name": "chat",
      "mode": "chat",
      "count": 24,
      "rate_per_sec": 0.4,
      "join_room": true
    }
  ]
}
```

## 10. 리포트 포맷 초안

```json
{
  "scenario": "steady_chat",
  "room": "loadgen_room_<seed>",
  "sessions": 64,
  "connected_sessions": 64,
  "authenticated_sessions": 64,
  "joined_sessions": 64,
  "success_count": 0,
  "error_count": 0,
  "throughput_rps": 0.0,
  "latency_ms": {
    "p50": 0.0,
    "p95": 0.0,
    "p99": 0.0,
    "max": 0.0
  },
  "transport": {
    "connect_failures": 0,
    "read_timeouts": 0,
    "disconnects": 0
  }
}
```

## 10.1 실제 wire 관찰 메모

- `haproxy -> gateway_app` 경로는 connect 직후 `MSG_HELLO`를 보장하지 않는다.
- handshake는 첫 `MSG_LOGIN_REQ`를 기다린 뒤 backend 연결을 열고 투명 브리지로 전환한다.
- join 성공 확인은 현 시점 기준 `MSG_STATE_SNAPSHOT`보다 시스템 `MSG_CHAT_BROADCAST`가 더 안정적이었다.
- 고정 room 재사용은 이전 run의 room history/state를 끌고 와 packet size/정책 이슈를 만들 수 있어 `unique_room_per_run`을 기본값으로 둔다.

## 11. 검증 계획

### 11.1 설계 검증

- 문서/CLI/리포트 포맷을 먼저 고정
- 기존 smoke/soak와 책임이 겹치지 않도록 구분

### 11.2 구현 검증

- 로컬 빌드: `tcp_loadgen` 단독 빌드 성공
- 로컬 stack:
  - baseline connect/login/join/chat
  - JSON report 출력 확인
- 최소 회귀:
  - 1세션
  - 다중 세션
  - timeout/error path

### 11.3 운영형 검증

- Docker stack against HAProxy frontend
- `tasks/quantitative-validation.md`의 mixed soak 항목에 실제 수치 기록

### 11.4 실제 완료 검증

- `pwsh scripts/build.ps1 -Config Release -Target tcp_loadgen`
- `pwsh scripts/deploy_docker.ps1 -Action up -Detached -Build`
- `python tests/python/verify_chat.py`
- `python tests/python/verify_pong.py`
- `build-windows\\Release\\tcp_loadgen.exe --host 127.0.0.1 --port 6000 --scenario tools/loadgen/scenarios/steady_chat.json --report build/loadgen/steady_chat.json`
- `build-windows\\Release\\tcp_loadgen.exe --host 127.0.0.1 --port 6000 --scenario tools/loadgen/scenarios/mixed_session_soak.json --report build/loadgen/mixed_session_soak.json`

실제 결과:

- `steady_chat`: `connected=24 authenticated=24 joined=24 success=155 errors=0 throughput_rps=8.60 p95_ms=14.11`
- `mixed_session_soak`: `connected=24 authenticated=24 joined=12 success=84 errors=0 throughput_rps=4.60 p95_ms=14.51`

후속 결함 처리:

- loadgen 반복 실행 중 드러난 `gateway_app` 종료 경로의 in-flight write buffer 수명 문제를 같은 브랜치에서 수정했다.
- 수정 후 same-stack repeated run(`steady -> mixed -> steady -> mixed`) 기준으로 gateway 컨테이너가 살아 있고 `double free` 로그가 재발하지 않음을 확인했다.

## 12. 1차 완료 기준

- `tcp_loadgen`이 실제 stack에 TCP 세션 다수를 붙일 수 있다.
- `steady_chat`, `mixed_session_soak` 2개 시나리오를 실행할 수 있다.
- JSON report에 latency/throughput/error 요약이 남는다.
- Docker stack 기준 실행 절차가 문서화된다.
