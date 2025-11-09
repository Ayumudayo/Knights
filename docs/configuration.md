# Configuration Guide

Knights 프로젝트는 `.env` 파일과 환경 변수를 사용해 서버 구성 요소를 설정한다. 일부 실험적 YAML 구성은 더 이상 유지하지 않으며, 모든 실행 파일이 공통 `.env` 또는 실행 디렉터리 내 `.env`를 자동으로 로드한다. 이 문서는 구성 원칙과 멀티 인스턴스 운영 시 주의 사항을 정리한다.

## 1. 구성 로딩 순서
1. 실행 파일과 동일한 디렉터리에 `.env`가 있으면 우선 로드한다.
2. 없을 경우 리포지토리 루트의 `.env`를 로드한다.
3. 프로세스 환경 변수는 항상 `.env` 값을 오버라이드한다.
4. 각 애플리케이션은 필요 시 추가 검증(예: 필수 키 누락)을 수행하고 기본값을 적용한다.

멀티 인스턴스 환경에서는 공통 `.env`를 공유하기보다 인스턴스별로 필요한 값만 export한 뒤 실행하는 방식을 권장한다.

## 2. 핵심 환경 변수
### 2.1 서버 공통
| 변수 | 설명 |
| --- | --- |
| `DB_URI` | PostgreSQL 연결 문자열 (server_app, wb_worker) |
| `DB_POOL_MIN`, `DB_POOL_MAX` | DB 커넥션 풀 크기 |
| `REDIS_URI` | Redis 연결 문자열 (공통) |
| `REDIS_POOL_MAX` | Redis 커넥션 풀 최대치 |
| `WRITE_BEHIND_ENABLED` | Redis Streams write-behind 활성화 |
| `USE_REDIS_PUBSUB` | Redis Pub/Sub 브로드캐스트 활성화 |
| `REDIS_CHANNEL_PREFIX` | Pub/Sub 채널 Prefix (`knights:` 등) |
| `GATEWAY_ID` | Gateway/Server 식별자 (Presence, Pub/Sub self-echo에 사용) |

### 2.2 server_app 설정
| 항목 | 기본값 | 설명 |
| --- | --- | --- |
| `SERVER_BIND_ADDR` | `0.0.0.0` | 서버 리스닝 주소 |
| `SERVER_PORT` | `5000` | 서버 리스닝 포트 |
| `PRESENCE_TTL_SEC` | `30` | Presence TTL (초) |
| `SERVER_ADVERTISE_HOST` | `127.0.0.1` | Load Balancer가 backend를 찾을 때 사용하는 외부 호스트명(예: VIP/프록시) |
| `SERVER_ADVERTISE_PORT` | listen 포트 | Load Balancer에 노출할 포트. 포트 매핑 시 외부 포트를 지정 |
| `SERVER_INSTANCE_ID` | `server-<timestamp>` | Instance Registry에 등록되는 고유 서버 ID |
| `SERVER_REGISTRY_PREFIX` | `gateway/instances` | Redis Instance Registry prefix (Load Balancer 설정과 일치해야 함) |
| `SERVER_REGISTRY_TTL` | `30` | Instance Registry TTL(초) |
| `SERVER_HEARTBEAT_INTERVAL` | `5` | Instance Registry heartbeat 주기(초) |
| `METRICS_PORT` | `9090` | HTTP metrics 포트(관측/대시보드 용도) |
| `WB_*` | - | write-behind batch, delay, DLQ 등 세부 옵션 |

### 2.3 gateway_app 전용
| 변수 | 기본값 | 설명 |
| --- | --- | --- |
| `GATEWAY_LISTEN` | `0.0.0.0:6000` | 게이트웨이 TCP 리스너 |
| `LB_GRPC_ENDPOINT` | `127.0.0.1:7001` | Load Balancer gRPC 엔드포인트 |
| `LB_GRPC_REQUIRED` | `0` | 1이면 LB 연결 실패 시 종료 |
| `LB_RETRY_DELAY_MS` | `3000` | 재연결 대기(ms) |

### 2.4 load_balancer_app 설정
| 항목 | 기본값 | 설명 |
| --- | --- | --- |
| `LB_GRPC_LISTEN` | `127.0.0.1:7001` | Load Balancer gRPC 리스너 주소 |
| `LB_BACKEND_ENDPOINTS` | `127.0.0.1:5000` | 쉼표 구분 정적 backend 목록 |
| `LB_INSTANCE_ID` | `lb-<timestamp>` | 인스턴스 ID (미지정 시 자동 생성) |
| `LB_SESSION_TTL` | `45` | sticky 세션 TTL(초) |
| `LB_BACKEND_FAILURE_THRESHOLD` | `3` | backend 실패 허용 횟수 |
| `LB_BACKEND_COOLDOWN` | `5` | backend 재시도 대기(초) |
| `LB_REDIS_URI` | (없음) | Redis Session Directory/registry 접근 URI, 미설정 시 메모리 모드 |
| `LB_DYNAMIC_BACKENDS` | `0` | 1이면 Redis Instance Registry에서 backend를 자동 수집 |
| `LB_BACKEND_REFRESH_INTERVAL` | `5` | 동적 backend 목록 재로딩 간격(초) |
| `LB_BACKEND_REGISTRY_PREFIX` | `gateway/instances` | registry 키 prefix, 서버 설정과 동일해야 함 |
| `LB_HEARTBEAT_INTERVAL` | `5` | Load Balancer heartbeat 주기(초) |

### 2.5 wb_worker 전용
| 변수 | 기본값 | 설명 |
| --- | --- | --- |
| `REDIS_STREAM_KEY` | `session_events` | write-behind Stream 이름 |
| `WB_GROUP`, `WB_CONSUMER` | `wb_group`, `wb_consumer` | Stream 소비자 그룹/ID |
| `WB_BATCH_MAX_EVENTS` | `100` | 배치당 최대 이벤트 수 |
| `WB_BATCH_MAX_BYTES` | `524288` | 배치당 최대 크기(Byte) |
| `WB_BATCH_DELAY_MS` | `500` | 배치 지연(ms) |
| `WB_DLQ_STREAM` | `session_events_dlq` | DLQ 스트림 이름 |
| `WB_RETRY_MAX` | `5` | 재시도 횟수 |
| `WB_RETRY_BACKOFF_MS` | `250` | 재시도 백오프(ms) |

## 3. 멀티 인스턴스 구성 팁
1. **Gateway 다중화**  
   - 각 인스턴스에 고유한 `GATEWAY_ID`를 부여한다.  
   - 동일 Redis를 사용한다면 `REDIS_CHANNEL_PREFIX`를 일치시켜야 self-echo 필터가 동작한다.
2. **Load Balancer 설정**  
   - Redis Session Directory를 사용하면 sticky routing 키가 Redis에 저장되므로 장애 시 in-memory fallback 또는 재바인딩 전략을 준비한다.
   - LB_DYNAMIC_BACKENDS=1을 사용하면 Redis Instance Registry(LB_BACKEND_REGISTRY_PREFIX) 정보를 사용해 Consistent Hash ring을 재구성하므로, heartbeat 실패 시 LB_BACKEND_ENDPOINTS로 즉시 폴백하도록 감시한다.

3. **Server 설정**  
   - Redis Session Directory를 사용하면 sticky routing 전체가 Redis에 의존하므로 장애 시 fallback 전략을 반드시 준비한다.
   - LB_DYNAMIC_BACKENDS=1을 켜면 Redis Instance Registry(LB_BACKEND_REGISTRY_PREFIX)를 주기적으로 조회해 Consistent Hash ring을 재구성하고, 장애 시 LB_BACKEND_ENDPOINTS 값으로 즉시 폴백한다.
   - 모든 서버 인스턴스에서 DB_URI, REDIS_URI, REDIS_CHANNEL_PREFIX를 일관되게 맞춰 세션/브로드캐스트 경로가 섞이지 않도록 한다.
   - SERVER_ADVERTISE_HOST/PORT는 Load Balancer가 접속하는 주소이며 SERVER_REGISTRY_PREFIX도 LB 설정과 일치해야 heartbeat가 정상적으로 갱신된다.
   - USE_REDIS_PUBSUB=1인 경우 브로드캐스트가 Redis에 의존하므로 채널 prefix와 self-echo 정책을 점검한다.
   - METRICS_PORT는 관측 도구만 접근하도록 방화벽·보안그룹을 설정한다.

4. **환경 파일 분리**  
   - 개발 단계에서는 루트 `.env` 하나를 공유해도 되지만, 운영에서는 구성 요소별 `.env.gateway`, `.env.lb`, `.env.server` 등을 만들어 필요한 값만 포함하도록 분리한다.  
   - `server/core/config::load_dotenv`는 실행 파일과 같은 디렉터리에 있는 `.env`를 우선하므로, 시스템 서비스 단위로 개별 `.env`를 둘 수 있다.

## 4. 검증 체크리스트
- `USE_REDIS_PUBSUB` 값이 서버 인스턴스마다 동일한가?
- `LB_BACKEND_ENDPOINTS`가 실제 server_app 인스턴스를 모두 포함하는가?
- Gateway/Load Balancer에서 Redis 연결이 실패할 경우 로그에 WARN이 남는지 확인.
- `.env`에 민감한 정보(DB 비밀번호 등)가 포함될 경우 운영 환경에서는 별도 비밀 관리(예: Azure Key Vault, AWS Secrets Manager)를 사용한다.

## 5. 참고 문서
- 서버 구조 개요: `docs/server-architecture.md`
- Gateway & Load Balancer 운영: `docs/ops/gateway-and-lb.md`
- 로드맵 및 우선순위: `docs/roadmap.md`
- Write-behind 상세: `docs/db/write-behind.md`
