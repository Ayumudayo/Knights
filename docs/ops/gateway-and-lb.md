# Gateway & Load Balancer 운영 가이드

이 문서는 Knights 분산 채팅 서버의 **Gateway → Load Balancer → Server** 흐름을 구성하고 운영하는 데 필요한 정보를 정리한다. 현재 구현은 gRPC 스트림과 Redis 기반 sticky 세션 매핑을 사용하며, 향후 다중 인스턴스/관측성 강화를 위한 TODO도 함께 기록한다.

```text
Client (TCP)
   │
   ▼
gateway_app ──(gRPC Stream)──▶ load_balancer_app ──(TCP)──▶ server_app
   │                                   │
   └────────── Redis (옵션) ◀──────────┘
```

## 1. 구성 요소 개요
### Gateway (`gateway_app`)
- Boost.Asio `Hive`를 이용해 클라이언트 TCP 세션을 관리한다.
- 각 세션은 Load Balancer의 `Stream` RPC와 1:1로 매핑된다.
- `/leave` 혹은 정상 종료 시 graceful close를 수행해 WARN 로그를 줄였다.

### Load Balancer (`load_balancer_app`)
- gRPC 서버로 Gateway와 통신하며 backend TCP 서버(`server_app`)를 선택한다.
- Consistent Hash + 실패 카운트를 사용해 sticky routing과 백엔드 격리를 동시 지원한다.
- LB_DYNAMIC_BACKENDS=1을 사용하면 Redis Instance Registry(LB_BACKEND_REGISTRY_PREFIX)에서 backend 목록을 읽어 Consistent Hash ring을 자동으로 재구성하고, 장애 시 LB_BACKEND_ENDPOINTS 값을 즉시 폴백으로 사용한다.
- Redis가 설정되면 `gateway/session/<client_id>`에 매핑을 저장하고 TTL이 지나면 자동 재할당한다.
- `gateway/instances/*` 키로 heartbeat를 기록해 다른 프로세스가 상태를 조회할 수 있다.

### Server (`server_app`)
- 실제 채팅 로직을 수행한다. Redis Pub/Sub(`USE_REDIS_PUBSUB=1`)를 활성화하면 여러 서버 인스턴스 간 메시지를 브로드캐스트한다.
- Room membership을 authoritative하게 관리하며 잘못된 라우팅은 `ROOM_MISMATCH` 오류로 감지된다.
- Redis Instance Registry에 backend heartbeat를 주기적으로 등록해 Load Balancer가 backend를 자동으로 발견하도록 한다.

## 2. 실행 절차
1. **데이터베이스/Redis 준비**  
   `DB_URI`, `REDIS_URI`를 `.env` 또는 환경 변수에 설정한다. Redis를 사용하지 않으면 sticky routing이 프로세스 로컬(비권장)로 떨어진다.
2. **server_app 실행**  
   ```powershell
   cmake --build build-msvc --target server_app
   .\build-msvc\server\Debug\server_app.exe
   ```
3. **load_balancer_app 실행**  
   ```powershell
   $env:LB_BACKEND_ENDPOINTS="127.0.0.1:5000"
   $env:LB_GRPC_LISTEN="127.0.0.1:7001"
   cmake --build build-msvc --target load_balancer_app
   .\build-msvc\load_balancer\Debug\load_balancer_app.exe
   ```
4. **gateway_app 실행**  
   ```powershell
   $env:LB_GRPC_ENDPOINT="127.0.0.1:7001"
   $env:GATEWAY_LISTEN="0.0.0.0:6000"
   cmake --build build-msvc --target gateway_app
   .\build-msvc\gateway\Debug\gateway_app.exe
   ```
5. **(옵션) devclient 실행**  
   ```powershell
   cmake --build build-msvc --target dev_chat_cli
   .\build-msvc\devclient\Debug\dev_chat_cli.exe
   ```

## 3. 환경 변수 요약
### Gateway
| 이름 | 설명 | 기본값 |
| --- | --- | --- |
| `GATEWAY_LISTEN` | TCP 바인딩 주소 | `0.0.0.0:6000` |
| `GATEWAY_ID` | 게이트웨이 인스턴스 ID | `gateway-default` |
| `LB_GRPC_ENDPOINT` | Load Balancer gRPC 주소 | `127.0.0.1:7001` |
| `LB_GRPC_REQUIRED` | 1이면 LB 연결 실패 시 종료 | `0` |
| `LB_RETRY_DELAY_MS` | 재연결 대기 시간(ms) | `3000` |

### Load Balancer
| 이름 | 설명 | 기본값 |
| --- | --- | --- |
| `LB_GRPC_LISTEN` | gRPC 리스너 주소 | `127.0.0.1:7001` |
| `LB_BACKEND_ENDPOINTS` | backend TCP 주소 목록 | `127.0.0.1:5000` |
| `LB_INSTANCE_ID` | 인스턴스 ID | 자동(`lb-<timestamp>`) |
| `LB_REDIS_URI` / `REDIS_URI` | Redis URI (선택) | 없음 |
| `LB_SESSION_TTL` | 세션 매핑 TTL(초) | `45` |
| `LB_BACKEND_FAILURE_THRESHOLD` | 격리 전 실패 허용 횟수 | `3` |
| `LB_BACKEND_COOLDOWN` | 재시도 대기(초) | `5` |
| `LB_HEARTBEAT_INTERVAL` | heartbeat 주기(초) | `5` |
| `LB_BACKEND_REFRESH_INTERVAL` | Registry 기반 backend 재조회 주기(초) | `5` |
| `LB_DYNAMIC_BACKENDS` | Redis Registry 기반 동적 backend 갱신 (1=활성) | `0` |
| `LB_BACKEND_REGISTRY_PREFIX` | backend registry prefix (server_app과 동일) | `gateway/instances` |

### Server
| 이름 | 설명 | 기본값 |
| --- | --- | --- |
| `SERVER_BIND_ADDR`, `SERVER_PORT` | TCP 리스너 | `0.0.0.0`, `5000` |
| SERVER_ADVERTISE_HOST, SERVER_ADVERTISE_PORT | Load Balancer가 접속할 외부 호스트/포트 | 127.0.0.1, listen 포트 |
| SERVER_INSTANCE_ID | Instance Registry에 등록할 고유 서버 ID | server-<timestamp> |
| `SERVER_REGISTRY_PREFIX` | Instance Registry prefix | `gateway/instances` |
| `SERVER_REGISTRY_TTL` | Instance Registry TTL(초) | `30` |
| `SERVER_HEARTBEAT_INTERVAL` | Registry heartbeat 주기(초) | `5` |
| `DB_URI` | PostgreSQL URI | 필수 |
| `REDIS_URI` | Redis URI | 선택 |
| `USE_REDIS_PUBSUB` | Redis Pub/Sub 활성 | `0` |
| `WRITE_BEHIND_ENABLED` | Redis Streams write-behind | `1` |
| `GATEWAY_ID`, `REDIS_CHANNEL_PREFIX` | Pub/Sub envelope 설정 | `gateway-default`, `knights:` |

환경 변수는 실행 파일과 동일 폴더의 `.env` 혹은 리포지토리 루트 `.env`에서도 자동으로 로드된다.

## 4. 모니터링 및 알람
- **Gateway / Load Balancer**
  - chat_subscribe_total, chat_self_echo_drop_total, chat_subscribe_last_lag_ms를 /metrics에서 수집한다. chat_subscribe_last_lag_ms 5분 p95가 200ms 이상이면 Redis Pub/Sub 경로를 우선 점검한다.
  - Load Balancer 로그의 pplied backend snapshot 라인을 기반으로 backend 리스트가 주기적으로 갱신되는지 확인한다. dynamic_backends=0으로 내려가면 즉시 원인을 확인한다.
  - Redis Session Directory TTL 만료율을 확인하기 위해 gateway/session/* 키 수를 SCAN해 급격한 감소/증가를 감시한다.
- **Server / Write-behind**
  - /metrics의 wb_batch_size, wb_commit_ms, wb_fail_total, wb_pending, wb_dlq_total을 대시보드에 노출하고, wb_pending이 200건 이상으로 5분간 유지되면 DLQ 상태를 점검한다.
  - RedisInstanceStateBackend 로그에서 heartbeat 실패가 연속으로 발생하면 SERVER_REGISTRY_PREFIX 설정과 Redis 상태를 동시에 확인한다.
- **권장 알람 임계치**
  - chat_subscribe_last_lag_ms p95 > 200ms (5분)
  - wb_fail_total 증가 또는 wb_pending > 500 (5분)
  - Redis ping latency p95 > 20ms, Postgres latency p95 > 50ms (5분)

## 5. 운영 체크리스트
## 5. 멀티 인스턴스 시나리오
1. **다중 gateway_app**  
   - 인스턴스마다 고유한 `GATEWAY_ID`를 지정한다.  
   - Redis Presence/Pub/Sub을 공유해도 self-echo 필터가 동작하도록 `REDIS_CHANNEL_PREFIX`를 동일하게 유지한다.
2. **다중 load_balancer_app**  
   - 현재는 정적 backend 목록을 사용한다. 여러 Load Balancer가 동시에 동작하려면 앞단에 L4(예: HAProxy)를 두거나 DNS 라운드로빈을 활용한다.  
- LB_DYNAMIC_BACKENDS=1을 사용하면 Redis Instance Registry(LB_BACKEND_REGISTRY_PREFIX)에서 backend 목록을 읽어 Consistent Hash ring을 자동으로 재구성하고, 장애 시 LB_BACKEND_ENDPOINTS 값을 즉시 폴백으로 사용한다.
3. **다중 server_app**  
    - LB_BACKEND_ENDPOINTS는 장애 시 사용할 정적 fallback 목록이며, SERVER_ADVERTISE_HOST/PORT는 Instance Registry에 올리는 주소와 일치해야 한다.
    - LB_BACKEND_ENDPOINTS는 장애 시 사용할 정적 fallback 목록이며, SERVER_ADVERTISE_HOST/PORT는 Instance Registry에 올리는 주소와 일치해야 한다.
    - Redis Pub/Sub(USE_REDIS_PUBSUB=1)을 사용할 경우 채널 prefix와 self-echo 정책을 맞추고, Pub/Sub 지연에 대한 경보를 설정한다.
    - Presence/Write-behind 경로는 Redis/DB 이중화 전략에 따라 장애 전환 절차와 모니터링 지표를 정의한다.
## 6. 향후 TODO
| 우선순위 | 항목 | 메모 |
| --- | --- | --- |
- LB_DYNAMIC_BACKENDS=1을 사용하면 Redis Instance Registry(LB_BACKEND_REGISTRY_PREFIX)에서 backend 목록을 읽어 Consistent Hash ring을 자동으로 재구성하고, 장애 시 LB_BACKEND_ENDPOINTS 값을 즉시 폴백으로 사용한다.
| [done][P1] | Redis 세션 폴백 강화 | Registry 폴백/로그 정리를 마쳐 sticky fallback 경로가 안정화 (docs/ops/distributed_routing_draft.md) |
| P2 | Redis Pub/Sub 정합성 강화 | 룸 생성/잠금에 대한 분산 락, 초기 캐시 동기화 전략 추가 |
| P2 | Sticky routing 통합 테스트 | 다중 서버·TTL 만료·Redis 장애 시나리오를 자동화 (`docs/ops/distributed_routing_draft.md`) |
| P3 | 관측성 확장 | Load Balancer 측 메트릭(세션 수, 재할당, backend 상태) 노출 |
| P3 | Gateway 인증 스켈레톤 | 토큰 검증, 세션 등록, 추후 로그인 서비스 연계 설계 (`docs/ops/distributed_routing_draft.md`) |

이 문서는 기능 구현이 진행될 때마다 업데이트해야 하며, 변경 내역은 `docs/roadmap.md`와 연동해 추적한다.
