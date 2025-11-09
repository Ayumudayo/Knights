# server_core 라이브러리

`core/` 디렉터리는 Knights 전반에서 공통으로 사용하는 C++20 static library `server_core`를 제공한다. 네트워킹(Hive/Connection), 동시성(TaskScheduler/LockedQueue), 스토리지(DbWorkerPool/Redis), 설정(load_dotenv) 등 서버 애플리케이션에 필요한 기반 기능을 모듈화했다. `server_app`, `gateway_app`, `load_balancer_app`, `dev_chat_cli` 모두 이 라이브러리를 통해 공통 코드를 공유한다.

## 디렉터리 구성
```text
core/
├─ include/server/core/
│  ├─ concurrent/   # JobQueue, ThreadManager, TaskScheduler
│  ├─ config/       # dotenv 로더, 옵션 파서
│  ├─ memory/       # BufferManager, MemoryPool
│  ├─ metrics/      # Counter/Gauge/Histogram SPI
│  ├─ net/          # Hive, Listener, Session, Connection
│  ├─ protocol/     # opcode, wire codec, error 정의
│  ├─ state/        # InstanceRegistry, presence 공유 컴포넌트
│  ├─ storage/      # DB/Redis 어댑터, write-behind 도우미
│  └─ util/         # log, crash_handler, service_registry, paths
├─ src/             # 각 헤더 구현
└─ tests/ (계획)    # 유닛/통합 테스트 예정
```

## 제공 기능
- **네트워크 스택**: `Hive`, `Listener`, `Session` 추상화를 통해 Boost.Asio 기반 I/O를 단일 스레드 루프 또는 다중 스레드 풀과 연동할 수 있다.
- **동시성 유틸리티**: `LockedQueue`, `LockedWaitQueue`, `TaskScheduler`가 백그라운드 작업과 지연 실행을 담당한다.
- **스토리지 계층**: `DbWorkerPool`은 PostgreSQL, Redis 작업을 비동기로 실행하며, `IConnectionPool` 인터페이스를 통해 다른 DB로 확장 가능한 구조를 제공한다.
- **Redis 지원**: SessionDirectory, write-behind Streams, Lua 스크립트 래퍼 등 Redis 기능을 공통 컴포넌트로 묶었다.
- **환경/서비스 관리**: `.env` 로드, ServiceRegistry 기반 의존성 주입, CrashHandler/로그 버퍼 등 운영 편의 기능을 포함한다.

## 빌드
```powershell
cmake --build build-msvc --target server_core
```

다른 타깃에서 사용하려면 `target_link_libraries(<target> PRIVATE server_core)`와 `target_include_directories(<target> PRIVATE core/include)`를 지정한다.

## 확장 로드맵
- Hive/Connection을 공용 네트워크 엔진으로 격상하여 Gateway·Load Balancer·게임 서버 등 다양한 프로세스에서 재사용.
- ECS, 플러그인, Lua/WASM 스크립팅 등 엔진형 기능을 실험하고 `docs/roadmap.md`에 우선순위를 관리.
- TaskScheduler/DbWorkerPool에 대한 단위 테스트를 추가하고, 종료 레이스·예외 경로를 커버하는 통합 테스트를 마련.
- 관측성 강화를 위해 Prometheus 메트릭/로그 구조를 표준화하고 `docs/ops/observability.md`를 통해 운영 가이드를 확장.
