# Server Architecture Overview

Knights 서버 스택은 `server_app`(채팅 서버), `gateway_app`(클라이언트 접점), `load_balancer_app`(세션 라우팅) 세 프로세스를 중심으로 구성된다. 공통 인프라는 `core/` 라이브러리에 존재하며, Redis·PostgreSQL·gRPC를 통해 확장 가능한 구조를 지향한다. 현재는 채팅 서버에 초점을 맞추지만, 향후 다른 장르의 서버 엔진으로 확장할 수 있도록 모듈화를 진행 중이다.

```text
┌───────────┐   Stream(gRPC)   ┌────────────────┐   TCP   ┌───────────┐
│ gateway   │ ───────────────▶ │ load_balancer  │ ───────▶ │ server    │
│ (Hive)    │                  │ (ConsistentHash│         │ (Chat)    │
└───────────┘                  └────────────────┘         └───────────┘
        │                               │                         │
        └────── Redis Presence / Sticky Session / PubSub ─────────┘
```

## 1. 컴포넌트
### server_app
- Boost.Asio 기반 TCP 리스너(`core::net::Acceptor`)와 세션(`core::net::Session`)을 사용해 프로토콜을 처리한다.
- `core::Dispatcher`에 opcode와 핸들러(`server::chat::handlers_*`)를 등록하여 메시지를 라우팅한다.
- `TaskScheduler`, `DbWorkerPool`, Redis 클라이언트 등을 서비스 레지스트리에 통해 주입받는다.
- Redis Streams write-behind, Redis Pub/Sub 브로드캐스트, Presence TTL 관리 등을 담당한다.

### gateway_app
- TCP 클라이언트 연결을 관리하고 초기 HELLO, heartbeat, 평문 payload를 Load Balancer로 전달한다.
- Load Balancer와의 gRPC 스트림이 끊기면 재연결하며, graceful 종료 시 WARN 로그 없이 닫히도록 조정했다.

- Gateway에서 전달된 세션을 동일 backend(server_app)로 보내기 위해 연결 매핑을 유지한다.
- Consistent Hashing과 Redis Session Directory를 결합해 sticky routing 키를 공유한다.
- LB_DYNAMIC_BACKENDS=1을 켜면 Redis Instance Registry(LB_BACKEND_REGISTRY_PREFIX)를 주기적으로 조회해 hash ring을 재구성하고, 장애 시 LB_BACKEND_ENDPOINTS 목록으로 폴백한다.
- Backend 헬스체크가 실패하면 LB_BACKEND_FAILURE_THRESHOLD/LB_BACKEND_COOLDOWN 설정에 따라 격리·재시도한다.
- Backend 연결 실패 시 실패 횟수를 기록해 일정 기간 격리한다.
- 인스턴스 heartbeat를 Redis `gateway/instances/*`에 기록해 상태를 외부에서 조회할 수 있다.

## 2. 부트스트랩 순서(server_app 기준)
1. `.env` 로딩 (`server/core/config/dotenv.hpp`).
2. `asio::io_context`, `TaskScheduler`, `DbWorkerPool`, `BufferManager` 초기화.
3. 환경 변수(`DB_URI`, `REDIS_URI`, `WRITE_BEHIND_ENABLED`, `USE_REDIS_PUBSUB` 등) 파싱.
4. 서비스 등록: DB/Redis 커넥션 풀, write-behind emitter, Redis Pub/Sub 구독자.
5. Redis Instance Registry에 backend heartbeat를 등록한다(SERVER_REGISTRY_PREFIX, SERVER_ADVERTISE_HOST/PORT).
6. 채팅 서비스 초기화(ChatService::init)를 실행해 핸들러/스토리지 의존성을 묶는다.
7. core::Acceptor를 기동하고 health-check, presence clean-up, metrics exporter를 시작한다.
8. OS 시그널(SIGINT/SIGTERM)을 수신하면 graceful shutdown 절차를 수행한다.

## 3. 메시지 플로우
1. 클라이언트가 Gateway에 TCP 연결 → Gateway가 HELLO 패킷을 수신 후 Load Balancer `Stream` RPC 생성.
2. Load Balancer가 sticky routing으로 backend 서버를 선택하고 TCP 소켓을 연다.
3. Gateway ↔ Load Balancer 간 gRPC 스트림은 클라이언트 payload를 backend로, backend payload를 클라이언트로 전달한다.
4. 서버는 opcode 기반으로 핸들러를 호출하고, 결과를 다시 Gateway를 통해 클라이언트로 돌려보낸다.
5. `USE_REDIS_PUBSUB=1`이면 서버는 룸 브로드캐스트를 Redis 채널에 발행하고, 다른 서버 인스턴스가 이를 수신해 로컬 세션에 전달한다.
6. `WRITE_BEHIND_ENABLED=1`이면 채팅 이벤트가 Redis Streams에 기록되고, `wb_worker`가 PostgreSQL에 영속화한다.

## 4. 다중 인스턴스 고려 사항
- **Gateway**: 인스턴스마다 `GATEWAY_ID`를 고유하게 설정한다. Redis Presence/Pub/Sub을 공유하며 self-echo 필터가 동작하도록 `REDIS_CHANNEL_PREFIX`는 통일한다.
- **Load Balancer**: 여러 인스턴스를 프런트에 두려면 L4 또는 DNS 로드밸런서를 추가한다. Redis가 없으면 sticky routing이 보장되지 않는다.
- **Server**: Redis Pub/Sub과 write-behind를 통해 메시지 및 이벤트를 동기화한다. Consistent hash ring이 서버 추가/제거에 자동 반응하도록 하는 기능은 로드맵으로 남아 있다.
- **Persistence**: PostgreSQL은 공유 데이터베이스로 사용되며, 향후 샤딩/파티셔닝 검토는 `docs/roadmap.md` 6) 항목에서 관리한다.

## 5. 운영 변수 요약
| 컴포넌트 | 주요 변수 | 설명 |
| --- | --- | --- |
| server_app | `SERVER_BIND_ADDR`, `SERVER_PORT` | TCP 리스너 설정 |
|  | `DB_URI`, `DB_POOL_MIN`, `DB_POOL_MAX` | PostgreSQL 연결 |
|  | `REDIS_URI`, `REDIS_POOL_MAX` | Redis 연결 |
|  | `WRITE_BEHIND_ENABLED`, `WB_*` | write-behind 제어 |
|  | `USE_REDIS_PUBSUB`, `GATEWAY_ID`, `REDIS_CHANNEL_PREFIX` | 분산 브로드캐스트 설정 |
|  | `PRESENCE_TTL_SEC` | Presence TTL / heartbeat 주기 |
|  | `METRICS_PORT` | HTTP 메트릭 포트 |
|  | `SERVER_ADVERTISE_HOST/PORT`, `SERVER_INSTANCE_ID` | Instance Registry에 등록할 공개 주소/ID |
|  | `SERVER_REGISTRY_PREFIX`, `SERVER_REGISTRY_TTL`, `SERVER_HEARTBEAT_INTERVAL` | Instance Registry heartbeat 설정 |
| gateway_app | `GATEWAY_LISTEN` | TCP 리스너 |
|  | `LB_GRPC_ENDPOINT` | Load Balancer 주소 |
|  | `LB_GRPC_REQUIRED`, `LB_RETRY_DELAY_MS` | 재연결 정책 |
| load_balancer_app | `LB_GRPC_LISTEN` | gRPC 리스너 |
|  | `LB_BACKEND_ENDPOINTS` | backend TCP 목록 |
|  | `LB_REDIS_URI`, `LB_SESSION_TTL` | Redis 세션 디렉터리 |
|  | `LB_BACKEND_REFRESH_INTERVAL` | Registry 기반 재조회 주기 |
|  | `LB_DYNAMIC_BACKENDS`, `LB_BACKEND_REGISTRY_PREFIX` | Registry 기반 동적 backend 구성 |
|  | `LB_BACKEND_FAILURE_THRESHOLD`, `LB_BACKEND_COOLDOWN` | 헬스 가드 |
|  | `LB_HEARTBEAT_INTERVAL` | heartbeat 주기 |

## 6. 향후 계획(발췌)
- `docs/roadmap.md`의 6) 항목에 다중 인스턴스 안정화 TODO가 정리되어 있다.  
  - Consistent hash 재구성 자동화  
  - Redis Pub/Sub 정합성 강화  
  - Sticky routing 통합 테스트  
  - Gateway 인증 확장
- 엔진화 로드맵(`docs/core-design.md`)에서는 Hive/Connection 추상화, ECS 조사, 플러그인 시스템 등의 장기 과제를 추적한다.

## 7. 참조
- 프로토콜 정의: `proto/gateway_lb.proto`, `proto/messages.proto`
- 운영 가이드: `docs/ops/gateway-and-lb.md`, `docs/ops/runbook.md`, `docs/ops/observability.md`
- 데이터 레이어: `docs/db/redis-strategy.md`, `docs/db/write-behind.md`
