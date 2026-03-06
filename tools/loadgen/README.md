# Load Generator

`stack_loadgen`은 기존 `haproxy -> gateway_app -> server_app` 경로를 대상으로 두고, transport-aware 시나리오를 하나의 headless binary로 실행하기 위한 도구다. 현재 구현 단계에서는 `tcp` transport만 지원한다.

## Build

Windows:

```powershell
pwsh scripts/build.ps1 -Config Release -Target stack_loadgen
```

Linux / Docker build dir:

```bash
cmake --build build-linux --target stack_loadgen
```

## Scenarios

- `tools/loadgen/scenarios/steady_chat.json`
  - 모든 세션이 동일 room에 join 후 steady chat echo latency를 측정
- `tools/loadgen/scenarios/mixed_session_soak.json`
  - `chat`, `ping`, `login_only` 세션을 섞어 mixed soak를 수행

필드:

- `sessions`
- `ramp_up_ms`
- `duration_ms`
- `room`
- `room_password`
- `unique_room_per_run`
- `message_bytes`
- `login_prefix`
- `connect_timeout_ms`
- `read_timeout_ms`
- `groups[]`

`groups[]` 예시:

```json
{
  "name": "chat",
  "transport": "tcp",
  "mode": "chat",
  "count": 24,
  "rate_per_sec": 2.0,
  "join_room": true
}
```

transport:

- `tcp`
- `udp` (planned)
- `rudp` (planned)

모드:

- `chat`
- `ping`
- `login_only`

## Run

예시:

```powershell
build-windows\Release\stack_loadgen.exe `
  --host 127.0.0.1 `
  --port 6000 `
  --scenario tools/loadgen/scenarios/steady_chat.json `
  --report build/loadgen/steady_chat.json `
  --verbose
```

Docker stack against HAProxy frontend:

```powershell
pwsh scripts/deploy_docker.ps1 -Action up -Detached -Build
build-windows\Release\stack_loadgen.exe `
  --host 127.0.0.1 `
  --port 6000 `
  --scenario tools/loadgen/scenarios/mixed_session_soak.json `
  --report build/loadgen/mixed_session_soak.json
```

## Report

실행 결과는 JSON으로 남는다. 핵심 필드:

- `connected_sessions`
- `authenticated_sessions`
- `joined_sessions`
- `success_count`
- `error_count`
- `operations.chat_*`
- `operations.ping_*`
- `throughput_rps`
- `latency_ms.p50/p95/p99/max`
- `transport.connect_failures/read_timeouts/disconnects`

## Notes

- `gateway_app` 경로는 접속 직후 `MSG_LOGIN_REQ`를 기다리므로, loadgen은 `HELLO`를 선행 조건으로 두지 않는다.
- 커스텀 room을 여러 세션이 공유하려면 `room_password`를 지정하는 편이 안전하다. 현재 서버 정책상 초기에 만들어진 room은 owner/invite 제약이 생길 수 있다.
- 기본값으로 `unique_room_per_run=true`를 사용해 run마다 room name에 seed suffix를 붙여 이전 run의 room history/state와 분리한다.
- chat rate는 기본 spam/mute 임계값 아래로 유지해야 한다. 제공된 샘플 시나리오는 이 기준을 반영한다.
- `udp`/`rudp` transport 필드는 schema에 예약돼 있지만, 현재 바이너리는 명시적으로 `tcp`만 구현한다.
