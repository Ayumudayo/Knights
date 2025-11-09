# load_balancer_app

`load_balancer/` 디렉터리는 Gateway로부터 gRPC 스트림을 받아 실제 TCP backend 서버로 라우팅하는 `load_balancer_app` 실행 파일을 포함한다. Consistent Hash 기반 sticky routing과 Redis 연동(Session Directory, Instance Registry)을 통해 다중 서버 인스턴스를 제어한다.

## 디렉터리 구성
```text
load_balancer/
├─ include/
│  └─ load_balancer/
│     ├─ load_balancer_app.hpp   # 앱 구성/실행, gRPC 서비스
│     └─ session_directory.hpp   # Redis 기반 세션 ↔ backend 매핑
└─ src/
   ├─ load_balancer_app.cpp      # 라우팅·헬스·Redis 연동 로직
   ├─ session_directory.cpp      # 세션 Sticky 매핑 구현
   └─ main.cpp                   # 엔트리 포인트
```

## 핵심 기능
- **Dynamic Backend Discovery**: `LB_DYNAMIC_BACKENDS=1` �� Redis Instance Registry(`LB_BACKEND_REGISTRY_PREFIX`)�� ���� backend ��ϵ��� 주기적으로 수집하고, 실패 시 정적 목록(`LB_BACKEND_ENDPOINTS`)으로 폴백한다.
- **Consistent Hash + Failover**: 클라이언트 ID를 기준으로 hash ring에서 backend를 선택하고, 연속 실패 시 `LB_BACKEND_FAILURE_THRESHOLD`와 `LB_BACKEND_COOLDOWN`을 사용해 일시 제외한다.
- **Redis Session Directory**: `LB_REDIS_URI` 혹은 전역 `REDIS_URI`가 설정되면 `gateway/session/<client_id>` 키를 이용해 sticky mapping을 공유한다. Redis가 없으면 프로세스 내 캐시로 동작한다.
- **Instance Registry Heartbeat**: `gateway/instances/*` 키에 현재 Load Balancer 상태를 heartbeat 형태로 기록하여 관측성을 제공한다.
- **헬스 게이팅**: backend TCP 연결이 실패할 때 실패 카운트를 증가시키고, 성공 시 초기화하여 불안정한 backend를 자동으로 격리한다.

## 환경 변수
| 이름 | 설명 | 기본값 |
| --- | --- | --- |
| `LB_GRPC_LISTEN` | Load Balancer gRPC 주소:포트 | `127.0.0.1:7001` |
| `LB_BACKEND_ENDPOINTS` | backend 서버 TCP 주소 목록(쉼표 구분) | `127.0.0.1:5000` |
| `LB_INSTANCE_ID` | 인스턴스 식별자(미설정 시 lb-타임스탬프) | 자동 생성 |
| `LB_REDIS_URI` / `REDIS_URI` | Redis 연결 문자열(선택) | 미설정 시 in-memory |
| `LB_SESSION_TTL` | Redis 세션 매핑 TTL(초) | `45` |
| `LB_BACKEND_FAILURE_THRESHOLD` | 격리 전 허용 실패 횟수 | `3` |
| `LB_BACKEND_COOLDOWN` | 격리 후 재시도까지 대기(초) | `5` |
| `LB_HEARTBEAT_INTERVAL` | 인스턴스 heartbeat 주기(초) | `5` |
| `LB_BACKEND_REFRESH_INTERVAL` | Registry 기반 재조회 주기(초) | `5` |
| `LB_DYNAMIC_BACKENDS` | Redis registry 기반 동적 backend 구성 (1=활성) | `0` |
| `LB_BACKEND_REGISTRY_PREFIX` | Instance Registry prefix (server_app과 동일) | `gateway/instances` |

## 빌드 및 실행
```powershell
cmake --build build-msvc --target load_balancer_app
.\build-msvc\load_balancer\Debug\load_balancer_app.exe
```

Gateway가 gRPC 스트림을 연결하려면 위 앱이 먼저 기동되어 있어야 하며, backend TCP 서버(`server_app`) 또한 `LB_BACKEND_ENDPOINTS` 목록과 일치하도록 구동돼야 한다.

## 멀티 인스턴스 메모
- Redis를 사용하면 여러 Load Balancer가 sticky session 정보를 공유할 수 있다. Redis가 없으면 프로세스마다 별도 캐시를 쓰므로 세션이 다른 backend로 이동할 수 있다.
- Consistent hash ring은 현재 정적 구성에 기반한다. Redis heartbeat(`gateway/instances/*`) 변화에 맞춘 동적 재구성은 `docs/roadmap.md`의 P1 항목으로 남아 있다.
- Redis Pub/Sub 기반 backend 브로드캐스트 확장은 `USE_REDIS_PUBSUB=1` 설정과 함께 향후 구현 예정이다.

자세한 설계와 운영 지침은 `docs/ops/gateway-and-lb.md`, `docs/server-architecture.md`를 참고한다.
