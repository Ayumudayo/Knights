# gateway_app

`gateway/` 디렉터리는 Knights 개발 환경에서 TCP 클라이언트 연결을 수용하고 gRPC 스트림으로 Load Balancer와 통신하는 `gateway_app` 실행 파일을 관리한다. Boost.Asio 기반 `Hive`와 `GatewayConnection` 추상화를 사용해 다수의 클라이언트 세션을 유지하며, 세션마다 Load Balancer `Stream` RPC를 열어 backend 서버로 패킷을 전달한다.

## 디렉터리 구성
```text
gateway/
├─ include/
│  └─ gateway/
│     ├─ auth/                  # 인증 훅(현재는 NoopAuthenticator)
│     ├─ gateway_app.hpp        # 앱 초기화 및 런타임 제어
│     └─ gateway_connection.hpp # TCP 세션 처리
└─ src/
   ├─ gateway_app.cpp           # 환경 로딩, Hive/gRPC 클라이언트 초기화
   ├─ gateway_connection.cpp    # 클라이언트 I/O 및 gRPC 라우팅
   └─ main.cpp                  # 엔트리 포인트
```

## 핵심 기능
- **세션 관리**: `GatewayConnection`이 TCP read/write 루프를 유지하고 `/leave`와 같은 정상 종료 시 INFO 로그만 남도록 조정했다.
- **gRPC 라우팅**: `gateway_lb.proto` 기반 `RouteMessage` 스트림을 사용해 Load Balancer와 양방향 통신한다.
- **Heartbeat & Presence**: 게이트웨이 자체 Heartbeat는 아직 없지만, 서버가 Redis Presence를 활용할 때 `GATEWAY_ID`가 self-echo 필터에 사용된다.
- **확장성**: `include/gateway/auth/` 하위에 토큰 검증 모듈을 삽입할 수 있는 인터페이스를 제공한다.

## 환경 변수
| 이름 | 설명 | 기본값 |
| --- | --- | --- |
| `GATEWAY_LISTEN` | 클라이언트를 수용할 TCP 주소:포트 | `0.0.0.0:6000` |
| `GATEWAY_ID` | 게이트웨이 인스턴스 식별자 (Presence·Pub/Sub self-echo 필터용) | `gateway-default` |
| `LB_GRPC_ENDPOINT` | Load Balancer gRPC 엔드포인트 | `127.0.0.1:7001` |
| `LB_GRPC_REQUIRED` | 1이면 LB 연결 실패 시 즉시 종료 | `0` |
| `LB_RETRY_DELAY_MS` | LB 재연결 대기 시간(ms) | `3000` |

`.env` 파일 또는 프로세스 환경 변수에 값을 지정하면 부팅 시 자동으로 로드된다.

## 빌드 및 실행
```powershell
cmake --build build-msvc --target gateway_app
.\build-msvc\gateway\Debug\gateway_app.exe
```

Load Balancer(`load_balancer_app`)가 먼저 실행되어 있어야 하며, `LB_GRPC_ENDPOINT`가 해당 인스턴스를 가리키도록 설정해야 한다.

## 추가 참고 자료
- gRPC 스키마: `proto/gateway_lb.proto`
- 전체 플로우 문서: `docs/server-architecture.md`, `docs/ops/gateway-and-lb.md`
- 다중 게이트웨이 구성 시 `GATEWAY_ID`를 인스턴스마다 고유하게 지정하고 Redis Pub/Sub 설정(`USE_REDIS_PUBSUB=1`)을 일치시켜야 한다.
