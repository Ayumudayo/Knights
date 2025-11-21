# 빌드 가이드

## 전제
- C++20 컴파일러(MSVC 19.3x+, GCC 11+, Clang 14+) (CMakeLists.txt:8)
- CMake 3.20+ (CMakeLists.txt:1)
- Boost 1.78+ (권장: 1.89) — manifest(`vcpkg.json`)에 선언되어 있으므로 `scripts/setup_vcpkg.ps1`(Windows) 또는 `scripts/setup_vcpkg.sh`(Linux) 실행 시 자동으로 설치됩니다. (CMakeLists.txt:81)
- Python 3 (선택: opcode 헤더 자동 생성) (CMakeLists.txt:97)
- vcpkg(필수): 저장소 루트의 `scripts/setup_vcpkg.*`가 `external/vcpkg/`에 공식 vcpkg를 부트스트랩하므로 `VCPKG_ROOT` 환경변수가 필요 없습니다. (scripts/build.ps1:72)

## 권장: vcpkg 매니페스트 빌드(FTXUI 포함)
1. scripts/setup_vcpkg.ps1(Windows) 또는 scripts/setup_vcpkg.sh(Linux/WSL)을 실행해 xternal/vcpkg/에 공식 vcpkg를 클론하고 manifest 의존성을 설치합니다.
2. 위 스크립트는 triplet(-Triplet/-t)만 지정하면 되며, 기본값은 Windows x64-windows, Linux x64-linux입니다.
3. 한 번 부트스트랩하면 VS2022, PowerShell, WSL 어느 환경에서든 동일한 xternal/vcpkg를 사용하므로 추가 환경변수 없이 
ind_package(Boost ...)가 성공합니다.
4. 필요 시 scripts/setup_vcpkg.* --skip-install로 재설치 없이 갱신할 수 있습니다.

### PowerShell(Windows)
- 자동 구성/빌드/실행(서버+클라): (scripts/build.ps1:168)
- 최초 1회: `pwsh scripts/setup_vcpkg.ps1 -Triplet x64-windows`로 vcpkg를 부트스트랩합니다.
- 빌드 + 실행 예시(scripts/build.ps1:168):
  - `scripts/build.ps1 -Run both -Config Debug`
  - `scripts/build.ps1 -Run server -Port 5000`
  - `scripts/build.ps1 -Run client -Port 5000`
- manifest 모드가 자동 활성화되므로 추가 파라미터 없이 Boost/Protobuf를 가져옵니다. RelWithDebInfo는 MSVC+vcpkg 조합에서 Debug로 강제 변환될 수 있습니다.

### Visual Studio 2022 (CMakePresets)
- 루트에 `CMakePresets.json`이 있으므로 VS 메뉴에서 **CMake > Change Configure Preset > windows-vcpkg**를 선택하면 됩니다.
- `scripts/setup_vcpkg.ps1`을 한 번 실행했다면 VS 메뉴에서 **CMake > Change Configure Preset > windows-vcpkg**만 선택하면 됩니다.
- preset은 `${sourceDir}/cmake/knights_vcpkg_toolchain.cmake`를 통해 `external/vcpkg`를 자동으로 찾으므로 `VCPKG_ROOT` 환경변수가 필요 없습니다.
- 기존 CMake 캐시에 수동 Boost 경로가 남아 있다면 **CMake > Delete Cache and Reconfigure**로 초기화하세요.
- CLI에서도 동일하게 사용 가능:
  ```powershell
  cmake --preset windows-vcpkg
  cmake --build --preset windows-vcpkg-relwithdebinfo --target server_app
  ```
### Linux/WSL
```bash
sudo apt-get update && sudo apt-get install -y build-essential cmake ninja-build
bash scripts/setup_vcpkg.sh -t x64-linux
cmake --preset linux-vcpkg-debug
cmake --build --preset linux-vcpkg-debug --target server_app
# 또는
./scripts/build.sh -c Debug -r all
```
- Ninja 대신 Makefiles를 쓰고 싶다면 `-g "Unix Makefiles"` 옵션을 추가하세요.
## 모듈 선택 빌드
루트 `CMakeLists.txt`는 각 모듈을 개별 옵션으로 제어한다. 기본값은 모두 ON이며 필요한 컴포넌트만 선택해 빌드할 수 있다.

| 옵션 | 기본 | 설명 |
| --- | --- | --- |
| `-DBUILD_SERVER_STACK=` | `ON` | `server/` 하위 라이브러리와 `server_app` 실행 파일. Load Balancer 및 write-behind 도구는 이 옵션이 켜져 있어야 한다 |
| `-DBUILD_GATEWAY_APP=` | `ON` | `gateway_app` 빌드 여부 |
| `-DBUILD_LOAD_BALANCER_APP=` | `ON` | `load_balancer_app` 빌드 여부 (`BUILD_SERVER_STACK` 필요) |
| `-DBUILD_DEVCLIENT_APP=` | `ON` | FTXUI 기반 CLI(`dev_chat_cli`) 빌드 여부 |
| `-DBUILD_WRITE_BEHIND_TOOLS=` | `ON` | `wb_worker`, `wb_emit`, `wb_dlq_replayer`, `wb_check` 빌드 여부 (`BUILD_SERVER_STACK` 필요) |
| `-DBUILD_SERVER_TESTS=` | `ON` | `tests/` 하위 타깃 빌드 여부 |

예시:
```powershell
cmake -S . -B build-core -DBUILD_GATEWAY_APP=OFF -DBUILD_LOAD_BALANCER_APP=OFF `
  -DBUILD_DEVCLIENT_APP=OFF -DBUILD_WRITE_BEHIND_TOOLS=OFF -DBUILD_SERVER_TESTS=OFF
cmake --build build-core --target server_core
```
Load Balancer만 확인하고 싶다면 `-DBUILD_GATEWAY_APP=OFF -DBUILD_DEVCLIENT_APP=OFF -DBUILD_WRITE_BEHIND_TOOLS=OFF` 등으로 조합하면 된다.

```

## VSCode 설정(요약)
- 확장: CMake Tools, C/C++(ms-vscode.cpptools)
- 절차
  - 킷 선택 후 Configure → Build.
  - vcpkg 사용 시 의존성/포함 경로가 자동 반영됩니다.

참고: 루트 `CMakeLists.txt`는 `CMAKE_EXPORT_COMPILE_COMMANDS=ON`을 설정합니다.

## 산출물/실행
- 바이너리
  - `server_app`: 테스트용 서버
  - `dev_chat_cli`: FTXUI 기반 CLI 클라이언트
  - `wb_worker`: Redis Streams → Postgres 배치 커밋 워커
  - `wb_dlq_replayer`: DLQ 재처리 도구(옵션)
  - `wb_emit`/`wb_check`: 스모크 테스트 보조 도구
- 실행 기본값
  - 서버: 기본 포트 `5000`
  - 클라: 인자 생략 시 `127.0.0.1:5000` 접속
- 스크립트 실행 예
  - `scripts/build.ps1  -Run both` → 서버 백그라운드 실행 후 클라 시작
  - 개별 실행은 `-Run server | client` 활용

## 빠른 시작(요약)
- 환경 파일: 루트 `.env`에 DB_URI/REDIS_URI 등 설정(예시는 `docs/getting-started.md` 참조)
- 서버 실행: `build-msvc/server/Debug/server_app.exe 5000`
- 워커 실행: `build-msvc/Debug/wb_worker.exe`
- 스모크: `scripts/smoke_wb.ps1`
- 메트릭: `METRICS_PORT` 설정 시 `curl http://127.0.0.1:9090/metrics`

## 릴리즈 번들 생성
- PowerShell(Windows)
  ```powershell
  scripts/build.ps1 -Config Release -Target server_app `
    -ReleasePackage -ReleaseZip `
    -ReleaseOutput artifacts `
    -ReleaseTargets server_app,gateway_app,load_balancer_app,dev_chat_cli
  ```
  - `artifacts/release-Release/`에 실행 파일과 README가 복사되고 `-ReleaseZip`을 켜면 동일 경로에 `.zip` 파일이 생성된다.
  - `-ReleaseTargets`는 쉼표 목록 또는 PowerShell 배열(@('server_app','wb_worker'))로 지정할 수 있다.
- Linux/WSL
  ```bash
  scripts/build.sh -c Release -r all \
    -R artifacts \
    -L "server_app,gateway_app,load_balancer_app" \
    -z artifacts/release-linux.tar.gz
  ```
  - `-R`은 복사 대상 디렉터리를 지정하고, `-z`는 동일 내용을 tar.gz로 압축한다.
  - `-L`을 생략하면 기본(`server_app,gateway_app,load_balancer_app,dev_chat_cli,wb_worker`)이 사용된다.
## 설치(옵션)
```
cmake --install build-msvc --config Debug --prefix "C:/server-core-sdk"  # Windows 예시
```
- 설치 후 포함 경로: `C:/server-core-sdk/include/server/core/...`
- 라이브러리 경로: `C:/server-core-sdk/lib`

## 코드 생성(opcodes)
- 소스 오브 트루스: `protocol/opcodes.json`
 - 생성 대상: `core/include/server/core/protocol/opcodes.hpp`
 - 수동 실행: `python tools/gen_opcodes.py protocol/opcodes.json core/include/server/core/protocol/opcodes.hpp`
