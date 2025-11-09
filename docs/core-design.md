# Core Design Notes

`server_core`는 Knights 서버 스택의 공통 인프라를 제공하는 C++20 정적 라이브러리다. 네트워크 I/O, 동시성, 스토리지 접근, 설정/운영 도구 등을 모듈화하여 채팅 서버, 게이트웨이, 로드밸런서, 향후 게임 서버까지 재사용 가능하도록 설계했다. 본 문서는 핵심 구성과 장기 로드맵을 정리한다.

## 1. 설계 목표
- **엔진화**: 채팅 서버뿐 아니라 향후 다른 장르의 실시간 서버로 확장할 수 있는 공통 기반 제공.
- **모듈성**: 네트워크, 동시성, 스토리지, 설정, 관측성 등을 독립 모듈로 나누어 필요한 부분만 링크 가능하도록 유지.
- **운영 친화성**: CrashHandler, ServiceRegistry, log 버퍼 등 운영 과정에서 필요한 툴을 기본으로 포함.
- **테스트 용이성**: 주요 컴포넌트(TaskScheduler, DbWorkerPool, Redis 어댑터 등)에 대해 단위/통합 테스트를 작성할 수 있는 구조를 지향.

## 2. 주요 모듈
### 2.1 네트워크 (`core::net`)
- `Hive`: Boost.Asio `io_context`를 래핑하여 공통 이벤트 루프를 제공.
- `Listener`: TCP acceptor. 다중 스레드에서 안전하게 start/stop 가능하도록 설계.
- `Connection`/`Session`: 비동기 read/write와 센더 큐를 관리. Wire codec과 dispatcher를 통해 메시지 라우팅.
- 향후 TODO: Hive/Connection을 별도 라이브러리로 분리하여 Gateway·Load Balancer·게임 서버가 동일 패턴을 사용하도록 일반화.

### 2.2 동시성 (`core::concurrent`)
- `LockedQueue`, `LockedWaitQueue`: background job 처리에 사용되는 thread-safe 큐.
- `ThreadManager`: 워커 스레드 풀을 관리.
- `TaskScheduler`: 지연/주기 실행을 지원. 현재 health check, presence cleanup, metrics 집계 등에 활용.
- 향후 TODO: TaskScheduler 종료 레이스와 재진입 케이스를 커버하는 단위 테스트 추가.

### 2.3 스토리지 (`core::storage`)
- `DbWorkerPool`: PostgreSQL/Redis 명령을 비동기 처리. 실패 시 롤백 및 재시도 전략을 지원.
- `redis::client`: Lua 스크립트, Streams, Pub/Sub, atomic set-if-equals 등 채팅 서버에서 필요한 Redis 기능 래퍼.
- 향후 TODO: DbWorkerPool과 Redis 클라이언트에 대한 통합 테스트, 모의 커넥션 풀 인터페이스 확장.

### 2.4 상태/서비스 (`core::state`, `core::util`)
- `InstanceRegistry`: Redis/In-memory backend를 통해 인스턴스 heartbeat를 관리.
- `ServiceRegistry`: 런타임 의존성을 주입하고 테스트 시 mock을 바꿔 끼울 수 있는 기반.
- `CrashHandler`: 비정상 종료 시 미니덤프와 로그를 `/logs/` 폴더에 기록.
- `log` 모듈: 버퍼드 로깅, 파일 회전 플래그, 콘솔 출력을 제공.

### 2.5 설정/관측성
- `config::load_dotenv`: 실행 파일 경로나 저장소 루트의 `.env`를 읽어 환경 변수를 설정.
- `metrics`: Counter/Gauge/Histogram 인터페이스를 제공하며, server_app은 `/metrics` HTTP 엔드포인트로 노출.
- 향후 TODO: Gateway·Load Balancer에서도 기본 메트릭을 노출하고, Prometheus exporter를 공통화.

## 3. 공용 패턴
1. **서비스 부트스트랩**  
   - `.env` 로드 → ServiceRegistry 초기화 → DbWorkerPool/Redis/PubSub/Write-behind 등록 → TaskScheduler 스케줄링 → 네트워크 리스너 시작.
2. **세션 라우팅**  
   - `core::net::Session`이 wire decoder로 opcode를 파싱 → `core::Dispatcher`에 등록된 핸들러를 호출 → 핸들러는 ServiceRegistry를 통해 필요한 의존성을 요청.
3. **백엔드 오프로드**  
   - 핸들러가 DB/Redis 작업을 수행해야 하면 `DbWorkerPool::enqueue`를 호출하여 백그라운드에서 실행.
4. **주기 작업**  
   - TaskScheduler가 health check, presence TTL 갱신, 로그 플러시, metrics 샘플링 등을 담당.

## 4. 엔진화 로드맵
| 구분 | 설명 | 현재 상태 |
| --- | --- | --- |
| Hive/Connection 공용화 | Gateway/Load Balancer/Server가 동일한 추상화를 사용하도록 정돈 | PoC 완료, 코드 정리 필요 |
| 모듈화된 인증 레이어 | `auth::IAuthenticator`를 확장해 토큰 검증·세션 레지스트리 연동 | NoopAuthenticator만 구현 |
| 플러그인 시스템 | Lua/WASM 등 스크립팅 혹은 DSO 로딩으로 확장 포인트 제공 | 조사 필요 |
| ECS/도메인 아키텍처 | 채팅 외 다른 게임 장르를 위한 상태 관리 프레임워크 | 조사 필요 |
| 관측성 표준화 | 로그 포맷, 메트릭 네임스페이스, 추적(Trace) 연동 | TODO |

로드맵 상세와 우선순위는 `docs/roadmap.md`에 정리되어 있으며, 이 문서는 핵심 방향성만 요약한다.

## 5. 테스트 전략
- `core/tests/`에 TaskScheduler, DbWorkerPool, Redis helper에 대한 단위 테스트를 추가 예정.
- gRPC와 TCP를 포함한 end-to-end 플로우는 별도 통합 테스트 하네스(예: `tests/integration/`)로 구성.
- PowerShell 기반 스모크 스크립트(`scripts/smoke_*.ps1`)와 CI 파이프라인을 연계하여 최소한의 회귀 테스트를 유지.

## 6. 참고 자료
- Sapphire 프로젝트 분석: `docs/sapphire_core_insights.md`
- 서버 전체 구조: `docs/server-architecture.md`
- 게이트웨이/로드밸런서 운영: `docs/ops/gateway-and-lb.md`
- 데이터 계층 전략: `docs/db/redis-strategy.md`, `docs/db/write-behind.md`
