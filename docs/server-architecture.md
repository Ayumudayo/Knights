# 서버 애플리케이션 구조

`server/` 모듈은 `core/` 라이브러리를 기반으로 한 단일 실행 파일(`server_app`)이며, 현재는 채팅 기능 중심의 Gateway + 도메인 로직을 한 프로세스에서 처리한다. Boost.Asio로 TCP 세션을 관리하고, 선택적으로 PostgreSQL · Redis를 연동한다.  
장기적으로는 동일 코어를 활용하는 다른 서버(매치메이커, 게임 월드 등)가 추가될 수 있으므로, 아래 “TODO” 절에 확장·일반화 항목을 기록해 둔다.

## 런타임 초기화
- **환경 변수 로드**: `run_server()` 시작 시 `.env` 파일을 우선 로드(`override=true`). 존재하지 않으면 OS 환경 변수 사용을 유지한다.
- **포트 결정**: 첫 번째 CLI 인자(없으면 5000)를 수신 포트로 사용한다.
- **핵심 객체 생성**  
  - `asio::io_context io` (네트워크 I/O 루프)  
  - `core::JobQueue` + `core::ThreadManager` (작업자 스레드)  
  - `core::BufferManager` (2KB × 1024)  
  - `core::Dispatcher` (opcode 라우팅 테이블)  
  - `core::SessionOptions`: `read_timeout_ms=60_000`, `heartbeat_interval_ms=10_000`, `recv_max_payload`/`send_queue_max`는 기본값 유지  
  - `core::SharedState`: 연결 수, 세션 ID 시퀀스 관리
- **외부 의존성 준비**
  - `DB_URI`가 설정되면 `server::storage::postgres::make_connection_pool()`로 libpqxx 기반 풀을 생성하고 health check 수행. 실패 시 프로세스를 중단한다.
  - `REDIS_URI`가 설정되면 `server::storage::redis::make_redis_client()`로 클라이언트를 생성한다. `HAVE_REDIS_PLUS_PLUS`가 정의되지 않았거나 생성 실패 시 스텁 구현으로 폴백한다.
- **서비스 구성**: `server::app::chat::ChatService`에 IO 컨텍스트, JobQueue, 선택적 DB/Redis 핸들을 주입한다. 이후 `register_routes()`로 Dispatcher에 메시지 핸들러를 등록한다.
- **Acceptor**: `core::Acceptor`가 TCP listen 후 세션을 생성한다. 세션 close 콜백에서 `ChatService::on_session_close()`가 호출되도록 설정한다.
- **스레드 풀**: CPU 코어 수(최소 1)만큼  
  - Worker: `ThreadManager::Start()`  
  - I/O: `std::thread`로 `io.run()` 호출
- **선택적 컴포넌트**
  - Redis Pub/Sub (`USE_REDIS_PUBSUB!=0`): `fanout:room:*` 패턴을 구독하여 타 게이트웨이에서 발행된 채팅을 재전송한다.
  - Metrics HTTP 서버 (`METRICS_PORT`): Prometheus 텍스트 포맷으로 런타임 지표를 노출한다(비동기 accept + on-demand 응답).
  - OS 시그널(SIGINT/SIGTERM) 처리: graceful shutdown 루틴 실행.

## 메시지 라우팅
`server/app/router.cpp`는 프로토콜 opcode를 `ChatService` 핸들러에 바인딩한다.

| Opcode | 핸들러 | 주요 역할 |
| ------ | ------ | --------- |
| `MSG_PING/MSG_PONG` | `on_ping` | 클라이언트 heartbeat 반사, Redis presence TTL 갱신 |
| `MSG_LOGIN_REQ` | `on_login` | 사용자 식별/닉네임 결정, DB audit 기록, write-behind 이벤트 |
| `MSG_JOIN_ROOM` | `on_join` | 방 입장/생성, 잠금 검사, membership 업데이트 |
| `MSG_CHAT_SEND` | `on_chat_send` | 일반 채팅, 슬래시 명령(`/refresh`, `/rooms`, `/who`, `/whisper`), 메시지 영속화 |
| `MSG_LEAVE_ROOM` | `on_leave` | 방 이탈, Redis presence 정리, write-behind 이벤트 |
| `MSG_WHISPER_REQ` | `on_whisper` | 로그인 사용자 간 귓속말, 수신 자격 검사 |
| 세션 종료 | `on_session_close` | 상태/룸 정리, Redis presence 정리, 종료 이벤트 |

메시지 처리 흐름은 다음과 같다.
1. `Session::do_read_body()`가 payload를 읽고 `Dispatcher`에 opcode를 전달한다.
2. Dispatcher가 `ChatService` 핸들러를 실행한다. 핸들러는 blocking 작업을 최소화하고, 필요 시 `JobQueue`를 통해 백그라운드 스레드에서 추가 작업을 수행한다.
3. 응답/브로드캐스트는 `Session::async_send()`로 프레이밍되어 송신된다. 송신 큐가 상한을 초과하면 세션을 종료한다.

## ChatService 내부 상태
`ChatService`는 방/사용자/세션 상태를 in-memory 구조(`state_`)로 추적한다.
- `rooms`: 방 이름 → 세션 약한 참조 집합 (`std::set<weak_ptr<Session>>`)
- `user`, `user_uuid`: 세션 → 닉네임/UUID
- `cur_room`: 세션 → 현재 방
- `authed`, `guest`: 인증 여부/게스트 모드
- `room_passwords`: 방별 비밀번호 해시(SHA 아님, `std::hash` 기반 간이 해시)
- `room_ids`: 방 이름 → DB room UUID 캐시
- `by_user`: 사용자명 → 세션 집합(귓속말 대상 탐색)
- `session_uuid`: 세션 → 서버에서 생성한 UUID v4 (write-behind 이벤트 키)

필요 시 std::mutex로 상태 전역을 직렬화한다. 방단위 스트랜드(`strand_for(room)`)는 향후 개선 여지를 위해 유지되고 있으나 아직 대부분의 처리는 글로벌 락으로 동작한다.

## DB 연동 (PostgreSQL)
`server/storage/postgres/connection_pool.cpp`는 libpqxx 기반으로 Repository/UnitOfWork를 구현한다.
- `make_connection_pool()`는 간단한 팩토리이며 실제 풀링은 구현되어 있지 않다(연결 시마다 `pqxx::connection` 생성). 향후 커넥션 풀을 도입할 수 있도록 `PoolOptions`를 받는다.
- Repository 책임
  - `PgUserRepository`: 게스트 생성, 마지막 로그인 IP 업데이트, ID/이름 조회
  - `PgRoomRepository`: 방 검색/생성, 케이스 무시 비교 지원
  - `PgMessageRepository`: 최근 메시지 조회, 메시지 저장, 마지막 ID 조회
  - `PgMembershipRepository`: 방 참여 상태 upsert, 마지막 읽은 메시지 반영
  - `PgSessionRepository`: 토큰 기반 세션 CRUD(현재 사용 범위 제한적)
- 주요 사용 시나리오
  - 로그인 시 게스트 사용자 생성 및 IP 로그 (`users` 테이블)
  - `/refresh` 또는 스냅샷 요청 시 최근 메시지와 membership.last_seen 조회
  - 메시지 전송 시 `messages` 테이블에 영속화 → 마지막 읽음 ID 업데이트
  - 방 입장/퇴장 시 membership 업데이트 및 Redis presence 동기화

## Redis 연동
Redis 사용은 선택 사항이며 `IRedisClient` 인터페이스로 캡슐화되어 있다.
- Presence: `presence:user:{uid}`(SETEX) · `presence:room:{room_uuid}`(SET) 키로 TTL 기반 온라인 상태를 추적한다(`PRESENCE_TTL_SEC`, 기본 30초).
- Write-behind: `WRITE_BEHIND_ENABLED=1`일 때 Redis Streams(`REDIS_STREAM_KEY`, 기본 `session_events`)에 이벤트를 기록한다. 필수 필드 `type`, `ts_ms`, `session_id`, `gateway_id` 외에 사용자/방 정보가 추가된다. `REDIS_STREAM_MAXLEN`, `REDIS_STREAM_APPROX`로 트림 동작 제어.
- Pub/Sub: `USE_REDIS_PUBSUB!=0`이면 `fanout:room:*` 패턴에 구독하고, 로컬 게이트웨이에서 발행한 메시지는 gateway id(`GATEWAY_ID`)로 필터링하여 재전송을 방지한다.
- 최근 메시지 캐시: Postgres에 기록된 채팅 메시지는 `room:{room_uuid}:recent` 리스트에 JSON 형태로 저장되며 최대 200개로 제한한다.

Redis 의존성이 없거나 연결 실패 시 모든 호출은 no-op 스텁으로 안전하게 폴백된다.

## 지표 및 모니터링
`METRICS_PORT`가 설정되면 간단한 HTTP 서버가 Prometheus 형태로 메트릭을 노출한다.
- ChatService 로컬 카운터: `chat_subscribe_total`, `chat_self_echo_drop_total`, `chat_subscribe_last_lag_ms` 등.
- `runtime_metrics::snapshot()` 기반 서버 코어 지표: accept/session/frame/dispatch/job_queue/memory_pool 등의 카운터·게이지, opcode별 dispatch 카운트.
- 지표 수집 시 멀티스레드 환경을 고려하여 atomic load 결과를 그대로 사용한다.

메트릭 서버는 별도 `io_context`와 스레드로 동작하며, 종료 시 stop()을 호출해 안전하게 정리한다.

## 스레딩 모델
- **네트워크 I/O**: `io_context`를 공유하는 N개의 스레드가 `Session` 입출력을 처리한다. 세션별 strand를 통해 read/write 콜백이 직렬화된다.
- **백그라운드 작업**: JobQueue + ThreadManager로 구성된 워커 풀이 CPU 바운드/DB/Redis 호출을 처리한다. 핸들러는 블로킹 작업을 큐에 넘겨 I/O 스레드를 가볍게 유지한다.
- **Metrics/Redis 구독**: 필요 시 추가 스레드(HTTP 서버 1개, Redis Pub/Sub 1개)가 생성된다.
- **종료 순서**: 시그널 수신 → Redis 구독 중지 → metrics `io_context` stop → acceptor stop → `io_context` stop → worker Stop → join.

## 환경 변수 요약
| 키 | 설명 | 기본값 |
|----|------|--------|
| `DB_URI` | PostgreSQL 접속 문자열 | (미설정) |
| `DB_POOL_MIN`, `DB_POOL_MAX` | (미사용 예약) 풀 최소/최대 연결 수 | 1 / 10 |
| `DB_CONN_TIMEOUT_MS`, `DB_QUERY_TIMEOUT_MS` | 접속/쿼리 타임아웃 힌트 | 5000 |
| `DB_PREPARE_STATEMENTS` | 준비된 쿼리 사용 여부 (`0`/그 외) | true |
| `REDIS_URI` | Redis 접속 문자열 | (미설정) |
| `REDIS_POOL_MAX` | Redis 풀(예정) 최대 연결 수 | 10 |
| `WRITE_BEHIND_ENABLED` | Redis Streams 기록 활성화 | false |
| `REDIS_STREAM_KEY` | write-behind 스트림 키 | `session_events` |
| `REDIS_STREAM_MAXLEN` | XADD maxlen | (무제한) |
| `REDIS_STREAM_APPROX` | maxlen ~ 옵션 (`1`=approx) | true |
| `PRESENCE_TTL_SEC` | presence:user TTL (초) | 30 |
| `REDIS_CHANNEL_PREFIX` | Pub/Sub 채널 prefix · presence 키 prefix | (빈 문자열) |
| `USE_REDIS_PUBSUB` | Pub/Sub 팬아웃 사용 여부 (`0`/그 외) | false |
| `GATEWAY_ID` | 게이트웨이 식별자 | `gw-default` |
| `WRITE_BEHIND_ENABLED` | Write-behind 스트림 사용 | false |
| `METRICS_PORT` | Prometheus HTTP 포트 | 0 (비활성) |
| `SNAPSHOT_RECENT_LIMIT` | 스냅샷 응답 메시지 수 상한 | 20 |
| `SNAPSHOT_FETCH_FACTOR` | 미읽은 메시지 조회 배수 | 3 |

환경 변수는 `.env`에 정의해 개발과 운영 구성을 분리할 수 있다.

## 남은 과제 / TODO
- `ThreadManager` + `JobQueue` 사용을 명확하게 설계(현재는 주로 로그인/채팅 핸들러에서 사용).
- 세션 종료 시 룸 스트랜드를 활용해 락 경합 완화.
- Redis presence 키와 Streams 형식을 문서화하고, 컨슈머 예제(`wb_worker`)를 추가.
- PostgreSQL 커넥션 풀을 실제 풀링 구현으로 교체하고, 트랜잭션 실패 시 롤백/재시도 로직을 강화.
- TLS 수용 및 인증(토큰 기반) 추가, 핸들러별 권한 정책 분리.
- 통합 테스트(`tests/`) 작성 및 자동 검증 파이프라인 구축.

본 문서는 `server/` 디렉터리의 현재 구현(2025-03 기준)에 맞춰 갱신되었으며, 기능 추가 시 해당 섹션을 다시 업데이트해야 한다.

## TODO (엔진화 대비)
- [ ] 멀티 서비스 구성을 위한 Hive/Connection 계층 도입 (게이트웨이/로드밸런서/다중 인스턴스 대비).
- [ ] 스크립트 또는 플러그인 기반 확장 포인트 정의(커맨드/세션 훅 등).
- [ ] 고부하 환경 시 세션 상태 분산을 위한 샤딩·파티션 전략 설계.
- [ ] 게임 서버 등 다른 도메인에 필요한 상태 동기화/권한 제어 기능 식별.
- [ ] 통합 진단(분산 트레이싱, SLA 기반 메트릭) 요구사항 정리.

