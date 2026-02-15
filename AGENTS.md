# PROJECT KNOWLEDGE BASE

Generated: 2026-02-16
Commit: b9d2dd7
Branch: Huge-Refactor

## Overview
Knights는 C++20 기반 분산 채팅 스택입니다: HAProxy(TCP) -> `gateway_app` -> `server_app`, 그리고 Redis Streams -> Postgres 경로를 비동기로 적재하는 `wb_worker`(write-behind)가 있습니다.

표준 런타임은 Linux(Docker) 풀스택(`docker/stack/`)이며, Windows는 개발(빌드/클라이언트) 용도로 둡니다.

## Structure
```
./
├─ core/                 # 공용 라이브러리(server_core)
├─ server/               # server_app(채팅 로직)
├─ gateway/              # gateway_app(엣지 세션/라우팅)
├─ tools/                # 코드 생성 + write-behind 도구/워커
├─ scripts/              # 빌드/도커 배치/스모크 스크립트
├─ docker/
│  ├─ stack/             # 풀스택 compose(권장)
│  └─ observability/     # Prometheus/Grafana 설정/대시보드
├─ docs/                 # 설계/운영 문서
├─ external/              # 3rd-party (imgui 등)
├─ Sapphire/              # 참고용 서브프로젝트(별도 레포; Knights 스택/빌드와 무관)
├─ prometheus/            # legacy Prometheus 샘플(표준은 docker/observability)
├─ proto/                # Protobuf 정의
├─ protocol/             # wire map(JSON) 등 프로토콜 소스
├─ tests/                # GTest + python 검증
└─ client_gui/           # (Windows) ImGui 기반 개발 클라이언트
```

## Code Map
| Symbol | Kind | Location | Role |
|---|---|---|---|
| `server::core::net::Hive` | class | `core/include/server/core/net/hive.hpp` | io_context 수명/실행 관리 |
| `server::core::metrics::MetricsHttpServer` | class | `core/include/server/core/metrics/http_server.hpp` | `/metrics` HTTP endpoint |
| `server::core::metrics::append_build_info` | fn | `core/include/server/core/metrics/build_info.hpp` | `knights_build_info` 메트릭 라인 출력 |
| `server::core::runtime_metrics::Snapshot` | struct | `core/include/server/core/runtime_metrics.hpp` | 프로세스 런타임 카운터 스냅샷 |
| `server::app::run_server` | fn | `server/src/app/bootstrap.cpp` | server_app 부트스트랩(설정/DI/리스너/루프) |
| `server::app::register_routes` | fn | `server/src/app/router.cpp` | opcode -> handler 라우팅 |
| `server::app::chat::ChatService` | class | `server/include/server/chat/chat_service.hpp` | 채팅 상태/핸들러 구현 |
| `server::app::chat::ChatHookPluginChain` | class | `server/src/chat/chat_hook_plugin_chain.hpp` | 채팅 훅 플러그인 체인(멀티 플러그인 + reload 폴링) |
| `server::app::chat::ChatHookPluginManager` | class | `server/src/chat/chat_hook_plugin_manager.hpp` | 단일 플러그인 로더(cache-copy + lock/sentinel) |
| `server::app::MetricsServer` | class | `server/src/app/metrics_server.cpp` | server_app `/metrics` endpoint |
| `gateway::GatewayApp` | class | `gateway/include/gateway/gateway_app.hpp` | 게이트웨이 라이프사이클/백엔드 선택 |
| `gateway::GatewayApp::run` | method | `gateway/src/gateway_app.cpp` | 게이트웨이 메인 루프(리스너/메트릭 포함) |
| `gateway::GatewayConnection` | class | `gateway/include/gateway/gateway_connection.hpp` | 클라이언트<->백엔드 브리지 |
| `WbWorker` | class | `tools/wb_worker/main.cpp` | Redis Streams -> Postgres write-behind |

## Key Flows
- Client -> Stack: `haproxy`(TCP) -> `gateway_app` -> `server_app`.
- Gateway routing: sticky(`SessionDirectory`) -> least-connections(`InstanceRecord.active_sessions`) backend selection.
- Distributed fanout: `server_app` can `psubscribe` to `${REDIS_CHANNEL_PREFIX}fanout:*` for room broadcasts.
- Chat hook plugin(실험): `MSG_CHAT_SEND` 경로에 hot-reload 가능한 플러그인 체인을 적용(파일명 순서; cache-copy + 선택적 lock/sentinel).
- Streams -> DB: `wb_emit`(`XADD`) -> `wb_worker`(`XREADGROUP`) -> Postgres `session_events`.
- Metrics: each service exposes Prometheus text format on `/metrics` (ports wired in `docker/stack/docker-compose.yml`).

## Where To Look
| Task | Location | Notes |
|---|---|---|
| Build presets / 옵션 | `CMakeLists.txt`, `CMakePresets.json` | `BUILD_*` 옵션, Windows(vcpkg) vs Linux(docker) 흐름 |
| Windows 빌드 | `scripts/build.ps1` | vcpkg bootstrap + preset configure/build |
| Docker 풀스택 | `docker/stack/docker-compose.yml`, `scripts/deploy_docker.ps1` | `observability` profile 포함 |
| Observability | `docker/observability/prometheus/prometheus.yml`, `docker/observability/grafana/dashboards/` | Grafana provisioning은 `docker/observability/grafana/provisioning/` |
| 서버 런타임 메트릭 | `core/include/server/core/runtime_metrics.hpp`, `server/src/app/metrics_server.cpp` | `/metrics` 텍스트 포맷 노출 |
| Chat hook plugin | `server/src/chat/chat_hook_plugin_*.{hpp,cpp}`, `server/plugins/` | 설정: `docs/configuration.md`, `server/README.md` |
| 게이트웨이 라우팅/세션 | `gateway/src/gateway_app.cpp`, `gateway/src/gateway_connection.cpp` | Redis Instance Registry + SessionDirectory |
| Write-behind 워커 | `tools/wb_worker/main.cpp` | Redis Streams -> Postgres + `/metrics`(옵션) |
| DB 마이그레이션 | `tools/migrations/runner.cpp`, `tools/migrations/*.sql` | `CREATE INDEX CONCURRENTLY`는 트랜잭션 밖 |
| 프로토콜 코드 생성 | `core/protocol/system_opcodes.json`, `tools/gen_opcodes.py`, `protocol/wire_map.json`, `tools/gen_wire_codec.py` | 생성 대상: `core/include/server/core/protocol/system_opcodes.hpp`, `core/include/server/wire/codec.hpp` |
| Opcode 문서/검증 | `tools/gen_opcode_docs.py`, `docs/protocol/opcodes.md` | system/game 전체 16-bit 중복 검증 + CI 체크 |
| CI | `.github/workflows/ci.yml` | Windows 빌드/테스트 + Linux Docker 풀스택 스모크 + opcode check |
| 테스트 | `tests/CMakeLists.txt`, `scripts/smoke_wb.ps1` | `ctest --preset windows-test` |

## Commands
```powershell
# Windows dev build
pwsh scripts/build.ps1 -Config Debug -Target server_app

# Full stack (Docker) + Observability
pwsh scripts/run_full_stack_observability.ps1

# Down (Docker)
pwsh scripts/deploy_docker.ps1 -Action down -Observability

# Tests
ctest --preset windows-test

# Opcode spec/doc check
python tools/gen_opcode_docs.py --check
```

## Conventions
- Naming/namespace: `docs/naming-conventions.md` (역할 기반 네이밍, 파일/함수 snake_case, 코드 심볼 ASCII).
- Repo 구조 원칙/금지사항: `docs/repo-structure.md`.
- Headers: `.hpp` + `#pragma once`, include 순서(표준 -> 서드파티 -> 프로젝트).
- CMake: 소스는 명시적으로 나열(= `file(GLOB ...)` 금지). 옵션은 `BUILD_SERVER_STACK`, `BUILD_GATEWAY_APP`, `BUILD_SERVER_TESTS`, `BUILD_WRITE_BEHIND_TOOLS` 중심.
- vcpkg: `vcpkg.json`의 `windows-dev` feature로 Windows 개발 의존성을 묶음(리눅스/도커 런타임은 시스템 패키지 기반).
- Metrics: `/metrics`는 Prometheus text format. 포트는 `METRICS_PORT`로 제어(서비스별 환경 변수로 주입).

## Anti-Patterns (This Repo)
- 코드/타깃/산출물/네임스페이스에 `Knights/knights/kproj` 문자열 사용 금지(`docs/repo-structure.md`).
- CMake에서 소스 수집을 `GLOB`로 처리 금지(리뷰 불가/증분 빌드 혼선).
- `core/`가 `server/`/`gateway/` 구현에 의존하도록 만들지 말 것(단방향 의존).
- `scripts/run_full_stack.*`, `scripts/run_haproxy_gateways.*`는 deprecated wrapper(직접 확장/추가 금지; `scripts/deploy_docker.ps1`/`scripts/run_full_stack_observability.ps1` 사용).
- 비밀정보/개인정보를 로그에 남기지 말 것(운영/샘플 토큰 포함). `.env`는 커밋 금지.

## Notes
- Docker 이미지: `knights-base:latest`(기반) -> `knights-app:local`(앱). `docker/stack`은 `observability` profile로 exporters/Prometheus/Grafana를 포함.
- Dispatch latency quantile(p95/p99)는 최근 구간에 샘플이 없으면 NaN이 나올 수 있음(정상). 트래픽 주입 후 확인.
- `prometheus/prometheus.yml`는 단일 job 샘플 구성(legacy)이며, 표준은 `docker/observability/prometheus/prometheus.yml`.
- `Sapphire/`는 참고용으로 동봉된 별도 프로젝트이며, Knights의 런타임/빌드 대상으로 취급하지 않는다.
- clangd/LSP 정밀 진단이 필요하면 `pwsh scripts/configure_windows_ninja.ps1`로 `build-windows-ninja/compile_commands.json`를 생성한 뒤, repo root에 `compile_commands.json`를 두면 된다(파일은 `.gitignore` 처리됨).
