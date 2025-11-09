# server_app (Chat Server)

`server/` 디렉터리는 Knights의 채팅 서버 실행 파일 `server_app`과 관련 헤더를 포함한다. `server_core` 라이브러리를 기반으로 TCP 세션 관리, 채팅 핸들러, Redis·PostgreSQL 연동, write-behind 이벤트 발행 등을 담당한다. 현재는 채팅 서버를 우선 지원하지만, 핵심 로직을 엔진 형태로 확장하려는 로드맵을 유지하고 있다.

## 디렉터리 구성
```text
server/
├─ include/server/
│  ├─ app/          # 앱 부트스트랩, 설정, 서비스 등록
│  ├─ chat/         # 채팅 핸들러 및 서비스
│  ├─ state/        # 인스턴스/Presence 상태 관리
│  └─ storage/      # DB, Redis, write-behind 어댑터
├─ src/
│  ├─ app/
│  ├─ chat/
│  ├─ state/
- **Instance Registry**: `SERVER_ADVERTISE_HOST/PORT`, `SERVER_REGISTRY_PREFIX`�� ���� Redis Instance Registry�� heartbeat�� �����Ͽ� Load Balancer�� backend�� �ڵ� ���ֵ��� �����Ѵ�.
│  └─ storage/
└─ tests/ (계획)
```

## 핵심 기능
- **세션/라우팅**: Boost.Asio 기반 세션과 `core::Dispatcher`로 opcode별 핸들러를 호출한다. 현재 `/login`, `/join`, `/leave`, `/chat`, `/whisper` 등을 제공한다.
- **권위 기반 메시지 처리**: 서버가 룸 멤버십을 authoritative하게 관리하며, 클라이언트가 잘못된 룸으로 채팅할 경우 `ROOM_MISMATCH` 오류를 반환한다.
- **Write-behind**: `WRITE_BEHIND_ENABLED=1`일 때 Redis Streams로 채팅 이벤트를 발행하고, `tools/wb_worker`가 비동기로 PostgreSQL에 적재한다.
- **TaskScheduler & Health Check**: 주기적인 DB/Redis 헬스 체크, presence 정리, 메트릭 갱신 등을 수행한다.
- **Redis Pub/Sub**: `USE_REDIS_PUBSUB=1`이면 `fanout:room:*` 채널로 메시지를 발행하여 다중 서버 인스턴스 간 채팅 동기화를 지원한다.

## 환경 변수
| 이름 | 설명 | 기본값 |
| --- | --- | --- |
| `SERVER_BIND_ADDR`, `SERVER_PORT` | TCP 리스너 주소/포트 | `0.0.0.0` / `5000` |
| `DB_URI` | PostgreSQL 연결 문자열 | (필수) |
| `DB_POOL_MIN`, `DB_POOL_MAX` | DB 커넥션 풀 크기 | `1` / `8` |
| `REDIS_URI` | Redis 연결 문자열 | 선택 |
| `REDIS_POOL_MAX` | Redis 커넥션 풀 최대치 | `16` |
| `WRITE_BEHIND_ENABLED` | Redis Streams write-behind 활성화 | `1` |
| `WB_*` | write-behind 배치/그룹/DLQ 설정 | `.env` 참고 |
| `USE_REDIS_PUBSUB` | Redis Pub/Sub 브로드캐스트 활성 | `0` |
| `PRESENCE_TTL_SEC` | Presence TTL(초) | `30` |
| `METRICS_PORT` | `/metrics` HTTP 포트 | `9090` |
| `SERVER_ADVERTISE_HOST/PORT`, `SERVER_INSTANCE_ID` | Instance Registry에 등록할 주소/ID | `127.0.0.1` / listen ��Ʈ |
| `SERVER_REGISTRY_PREFIX`, `SERVER_REGISTRY_TTL`, `SERVER_HEARTBEAT_INTERVAL` | Instance Registry heartbeat/TTL 설정 | `gateway/instances`, `30`, `5` |

`.env` 파일을 루트 혹은 실행 파일 경로에 두면 자동으로 로드되며, 환경 변수로도 덮어쓸 수 있다.

## 빌드 및 실행
```powershell
cmake --build build-msvc --target server_app
.\build-msvc\server\Debug\server_app.exe
```

Redis/DB 연결이 필요하면 실행 전에 환경 변수를 올바르게 지정해야 한다. 빠른 실행은 `scripts/build.ps1 -Run server`로 자동화할 수 있다.

## 운영 노트
- Redis Pub/Sub이 비활성(`USE_REDIS_PUBSUB=0`)이면 단일 서버 인스턴스로만 브로드캐스트된다. 다중 인스턴스 운영 시 1로 설정하고 `GATEWAY_ID`, `REDIS_CHANNEL_PREFIX`를 조정한다.
- TaskScheduler, DbWorkerPool, Redis health check 등 핵심 인프라 모듈은 `server_core`에 위치하며 다른 서버 타입으로 재사용할 수 있다.
- 엔진화 로드맵, Hive/Connection 확장, ECS 검토 등은 `docs/core-design.md`, `docs/roadmap.md`에서 관리한다.
- `scripts/smoke_server.ps1`(예정)과 수동 devclient 테스트를 조합해 `/join`·`/leave` 스냅샷 반영과 Pub/Sub 전달을 확인할 수 있다.
