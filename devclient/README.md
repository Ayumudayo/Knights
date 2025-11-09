# dev_chat_cli (Developer Client)

`devclient/` 디렉터리는 FTXUI 기반 터미널 클라이언트 `dev_chat_cli`를 제공한다. Gateway를 통해 서버 플로우를 테스트하기 위한 개발 도구로, 로그인/룸 이동/귓속말/메트릭 확인 등 기본 상호작용을 지원한다. 채팅 서버가 범용 엔진으로 확장되더라도 최소한의 TCP·프로토콜 검증 도구로 활용할 수 있도록 유지한다.

## 디렉터리 구성
```text
devclient/
├─ include/client/app/
│  ├─ command_processor.hpp   # 명령 파싱과 실행 분기
│  ├─ session_state.hpp       # 로컬 세션 캐시
│  └─ ui_components.hpp       # FTXUI 컴포넌트 헬퍼
├─ src/app/
│  ├─ command_processor.cpp
│  ├─ session_state.cpp
│  └─ ui/
├─ src/net_client.cpp         # TCP 연결 및 wire 인코딩
└─ src/main.cpp               # 엔트리 포인트
```

## 주요 기능
- `/login <name>`으로 손쉽게 게스트 로그인 후 `/join lobby` 등 룸 이동을 확인할 수 있다.
- 룸 이동 시 서버에서 즉시 내려오는 스냅샷을 반영하여 새로고침 없이 UI를 갱신한다.
- `Esc` 또는 `Ctrl+C`로 종료할 때 `/leave`와 소켓 shutdown을 전송하여 게이트웨이 로그가 WARN으로 남지 않도록 처리했다.
- 향후 인증 토큰/플러그인 명령이 추가되더라도 `CommandProcessor`만 확장하면 되도록 분리되어 있다.

## 환경 변수
| 이름 | 설명 | 기본값 |
| --- | --- | --- |
| `DEVCLIENT_HOST` | 접속할 Gateway 호스트 | `127.0.0.1` |
| `DEVCLIENT_PORT` | 접속할 Gateway 포트 | `6000` |

`.env`가 루트에 존재하면 자동으로 읽어들인다. 별도 인스턴스를 시험하려면 프로세스 환경 변수를 직접 지정해도 된다.

## 빌드 및 실행
```powershell
cmake --build build-msvc --target dev_chat_cli
.\build-msvc\devclient\Debug\dev_chat_cli.exe
```

FTXUI는 `vcpkg` 매니페스트에 포함되어 있으며, `cmake --preset` 또는 `scripts/build.ps1`를 사용해 의존성을 자동 설치할 수 있다.

## 테스트 시 참고
- 룸 이동 후 로비 목록에 즉시 반영되지 않으면 서버에서 내려온 스냅샷 패킷이 도착했는지 로그(`devclient.log`)로 확인한다.
- 다중 게이트웨이 실험 시 `GATEWAY_ID`가 다른 서버에 연결되는지 확인하고, Pub/Sub 브로드캐스트가 도착하면 UI가 실시간으로 갱신되는지 검증한다.
- `/rooms`, `/who`, `/refresh` 명령은 서버 authoritative 데이터를 기준으로 갱신하므로 클라이언트 측 상태와 불일치가 발생하면 서버 쪽 버그일 가능성이 높다.
